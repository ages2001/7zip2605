/* Streaming Deflate/Deflate64 decoder for segmented 16-bit targets.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include "deflate_stream.h"
#include "paged_dictionary.h"
#include "../../C/7zCrc.h"
#if defined(__WATCOMC__) && defined(__WINDOWS__)
#include <windows.h>
#endif
#define BLOCK 4096
/* Retain the complete 32/64 KiB history. Small independent RAM pages avoid
   segment-sized allocations; real-mode Windows uses the paged dictionary. */
#define HISTORY_PAGE 4096
#define HISTORY_PAGES (65536UL / HISTORY_PAGE)
#define MAX_BITS 15
/* Canonical codes are represented by lengths and sorted symbols. This avoids
   segment-sized lookup tables and never requires an unaligned word access. */
typedef struct { unsigned count[16], symbol[288]; } Tree;
typedef struct {
 ISeekInStreamPtr input; ISeqOutStreamPtr output;
 Byte *in, *out, *history[HISTORY_PAGES];
 Z7Win16Dictionary *dictionary;
 unsigned inPos,inSize,outSize,historyPos,bits,bufferSize,historyPages;
 UInt32 historyMask;
 UInt32 bitBuffer,crc;
 UInt64 packedLeft,outputLeft,produced;
 SRes error;
 Tree literal,distance,code;
} Inflate;
static unsigned readBits(Inflate *s,unsigned n){
 unsigned result;
 if(s->error!=SZ_OK)return 0;
 while(s->bits<n){
  if(s->inPos==s->inSize){
   size_t size;
   if(!s->packedLeft){s->error=SZ_ERROR_INPUT_EOF;return 0;}
   size=s->packedLeft>s->bufferSize?s->bufferSize:(size_t)s->packedLeft;
   s->error=ISeekInStream_Read(s->input,s->in,&size);
   if(s->error!=SZ_OK)return 0;
   if(!size){s->error=SZ_ERROR_INPUT_EOF;return 0;}
   s->packedLeft-=size;s->inPos=0;s->inSize=(unsigned)size;
  }
  s->bitBuffer|=(UInt32)s->in[s->inPos++]<<s->bits;s->bits+=8;
 }
 result=(unsigned)(s->bitBuffer&(((UInt32)1<<n)-1));
 s->bitBuffer>>=n;s->bits-=n;return result;
}
/* Reject oversubscribed trees. Only a one-symbol length-one literal/distance
   tree may be incomplete; the code-length alphabet must be complete. An
   empty distance tree is legal until a length/distance pair uses it. */
static SRes makeTree(Tree *t,const Byte *lengths,unsigned n,int kind){
 unsigned i,offset[16],max=0; long left=1;
 memset(t,0,sizeof(*t));
 for(i=0;i<n;i++){if(lengths[i]>15)return SZ_ERROR_DATA;t->count[lengths[i]]++;if(lengths[i]>max)max=lengths[i];}
 if(!max)return kind==2?SZ_OK:SZ_ERROR_DATA;
 t->count[0]=0;
 for(i=1;i<=15;i++){left=left*2-t->count[i];if(left<0)return SZ_ERROR_DATA;}
 if(left && (kind==0 || max!=1))return SZ_ERROR_DATA;
 offset[1]=0;for(i=1;i<15;i++)offset[i+1]=offset[i]+t->count[i];
 for(i=0;i<n;i++)if(lengths[i])t->symbol[offset[lengths[i]]++]=i;
 return SZ_OK;
}
static unsigned symbol(Inflate *s,const Tree *t){
 unsigned len,code=0,first=0,index=0;
 for(len=1;len<=15;len++){
  unsigned count=t->count[len];code|=readBits(s,1);
  if(s->error!=SZ_OK)return 0;
  if(code>=first && code-first<count)return t->symbol[index+code-first];
  index+=count;first=(first+count)<<1;code<<=1;
 }
 s->error=SZ_ERROR_DATA;return 0;
}
static SRes flush(Inflate *s){
 if(s->outSize && ISeqOutStream_Write(s->output,s->out,s->outSize)!=s->outSize)return SZ_ERROR_WRITE;
 s->outSize=0;return SZ_OK;
}
static SRes emit(Inflate *s,Byte value){
 if(!s->outputLeft)return SZ_ERROR_DATA;
 if(s->dictionary){
  Z7Win16_DictionarySet(s->dictionary,s->historyPos,value);
  RINOK(Z7Win16_DictionaryStatus(s->dictionary))
 }else s->history[s->historyPos/HISTORY_PAGE][s->historyPos%HISTORY_PAGE]=value;
 s->historyPos=(s->historyPos+1)&s->historyMask;
 s->out[s->outSize++]=value;s->produced++;s->outputLeft--;
 if(s->outSize==s->bufferSize){s->crc=CrcUpdate(s->crc,s->out,s->outSize);return flush(s);}
 return SZ_OK;
}
static SRes tables(Inflate *s,unsigned type){
 Byte lens[320]; unsigned i,hlit,hdist,hclen,total;
 static const Byte order[19]={16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
 if(type==1){
  for(i=0;i<288;i++)lens[i]=(Byte)(i<144?8:i<256?9:i<280?7:8);
  RINOK(makeTree(&s->literal,lens,288,1))
  memset(lens,5,32);return makeTree(&s->distance,lens,32,2);
 }
 hlit=257+readBits(s,5);hdist=1+readBits(s,5);hclen=4+readBits(s,4);
 if(s->error!=SZ_OK)return s->error;
 if(hlit>286)return SZ_ERROR_DATA;
 memset(lens,0,19);
 for(i=0;i<hclen;i++)lens[order[i]]=(Byte)readBits(s,3);
 if(s->error!=SZ_OK)return s->error;
 RINOK(makeTree(&s->code,lens,19,0))
 total=hlit+hdist;i=0;
 while(i<total){
  unsigned v=symbol(s,&s->code),repeat;Byte value;
  if(s->error!=SZ_OK)return s->error;
  if(v<16){lens[i++]=(Byte)v;continue;}
  if(v==16){if(!i)return SZ_ERROR_DATA;value=lens[i-1];repeat=3+readBits(s,2);}
  else if(v==17){value=0;repeat=3+readBits(s,3);}
  else if(v==18){value=0;repeat=11+readBits(s,7);}
  else return SZ_ERROR_DATA;
  if(s->error!=SZ_OK)return s->error;
  if(repeat>total-i)return SZ_ERROR_DATA;
  while(repeat--)lens[i++]=value;
 }
 if(!lens[256])return SZ_ERROR_DATA;
 RINOK(makeTree(&s->literal,lens,hlit,1))
 return makeTree(&s->distance,lens+hlit,hdist,2);
}
static SRes decode(Inflate *s){
 static const unsigned lengthBase[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
 static const Byte lengthBits[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
 static const unsigned distanceBase[32]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577,32769U,49153U};
 unsigned final=0;
 while(!final){
  unsigned type;
  final=readBits(s,1);type=readBits(s,2);
  if(s->error!=SZ_OK)return s->error;
  if(type==0){
   unsigned n,inverse;s->bitBuffer=0;s->bits=0;
   n=readBits(s,16);inverse=readBits(s,16);
   if(s->error!=SZ_OK)return s->error;
   if((n^inverse)!=65535U)return SZ_ERROR_DATA;
   while(n--){Byte v=(Byte)readBits(s,8);if(s->error!=SZ_OK)return s->error;RINOK(emit(s,v))}
  }else{
   if(type==3)return SZ_ERROR_DATA;
   RINOK(tables(s,type))
   for(;;){
    unsigned v=symbol(s,&s->literal),pos;UInt32 len,dist;
    if(s->error!=SZ_OK)return s->error;
    if(v<256){RINOK(emit(s,(Byte)v))continue;}
    if(v==256)break;
    if(v>285)return SZ_ERROR_DATA;
    v-=257;
    if(v==28&&s->historyMask==65535UL)len=3UL+readBits(s,16);
    else len=(UInt32)lengthBase[v]+readBits(s,lengthBits[v]);
    v=symbol(s,&s->distance);if(s->error!=SZ_OK)return s->error;
    if(v>=(s->historyMask==65535UL?32:30))return SZ_ERROR_DATA;
    dist=(UInt32)distanceBase[v]+readBits(s,v<4?0:v/2-1);
    if(s->error!=SZ_OK)return s->error;
    if((UInt64)dist>s->produced || (UInt64)len>s->outputLeft)return SZ_ERROR_DATA;
    while(len--){
     Byte value;pos=(unsigned)((s->historyPos-dist)&s->historyMask);
     value=s->dictionary?Z7Win16_DictionaryGet(s->dictionary,pos):s->history[pos/HISTORY_PAGE][pos%HISTORY_PAGE];
     RINOK(emit(s,value))
    }
   }
  }
 }
 if(s->outputLeft || s->packedLeft || s->inPos!=s->inSize)return SZ_ERROR_DATA;
 s->crc=CrcUpdate(s->crc,s->out,s->outSize);return flush(s);
}
static SRes inflateWindow(ISeekInStreamPtr input,ISeqOutStreamPtr output,
 UInt64 packedSize,UInt64 unpackedSize,ISzAllocPtr alloc,UInt32 *crc,int paged,UInt32 windowSize){
 Inflate *s;SRes result,cleanup;unsigned i;
 s=(Inflate *)ISzAlloc_Alloc(alloc,sizeof(*s));if(!s)return SZ_ERROR_MEM;
 memset(s,0,sizeof(*s));s->input=input;s->output=output;s->packedLeft=packedSize;
 s->outputLeft=unpackedSize;s->crc=CRC_INIT_VAL;s->bufferSize=paged?512:BLOCK;
 s->historyMask=windowSize-1;s->historyPages=(unsigned)(windowSize/HISTORY_PAGE);
 s->in=(Byte *)ISzAlloc_Alloc(alloc,s->bufferSize);s->out=(Byte *)ISzAlloc_Alloc(alloc,s->bufferSize);
 result=s->in&&s->out?SZ_OK:SZ_ERROR_MEM;
 if(result==SZ_OK){
  if(paged)result=Z7Win16_DictionaryCreate(&s->dictionary,windowSize,alloc);
  else for(i=0;i<s->historyPages;i++){
   s->history[i]=(Byte *)ISzAlloc_Alloc(alloc,HISTORY_PAGE);
   if(!s->history[i]){result=SZ_ERROR_MEM;break;}
  }
 }
 if(result==SZ_OK)result=decode(s);
 cleanup=Z7Win16_DictionaryDestroy(s->dictionary,alloc);
 if(result==SZ_OK)result=cleanup;
 if(result==SZ_OK && crc)*crc=CRC_GET_DIGEST(s->crc);
 ISzAlloc_Free(alloc,s->in);ISzAlloc_Free(alloc,s->out);
 for(i=0;i<s->historyPages;i++)ISzAlloc_Free(alloc,s->history[i]);
 ISzAlloc_Free(alloc,s);return result;
}
SRes Z7Win16_InflateWithCache(ISeekInStreamPtr input,ISeqOutStreamPtr output,
 UInt64 packedSize,UInt64 unpackedSize,ISzAllocPtr alloc,UInt32 *crc,int paged){
 return inflateWindow(input,output,packedSize,unpackedSize,alloc,crc,paged,32768UL);
}
SRes Z7Win16_Inflate64WithCache(ISeekInStreamPtr input,ISeqOutStreamPtr output,
 UInt64 packedSize,UInt64 unpackedSize,ISzAllocPtr alloc,UInt32 *crc,int paged){
 return inflateWindow(input,output,packedSize,unpackedSize,alloc,crc,paged,65536UL);
}
static int usePagedHistory(void){
#if defined(__WATCOMC__) && defined(__WINDOWS__)
 return !(GetWinFlags()&WF_PMODE);
#else
 return 0;
#endif
}
SRes Z7Win16_Inflate64(ISeekInStreamPtr input,ISeqOutStreamPtr output,
 UInt64 packedSize,UInt64 unpackedSize,ISzAllocPtr alloc,UInt32 *crc){
 return Z7Win16_Inflate64WithCache(input,output,packedSize,unpackedSize,alloc,crc,usePagedHistory());
}
SRes Z7Win16_Inflate(ISeekInStreamPtr input,ISeqOutStreamPtr output,
 UInt64 packedSize,UInt64 unpackedSize,ISzAllocPtr alloc,UInt32 *crc){
 /* Protected modes and DOS retain RAM history; real-mode Windows pages it. */
 return Z7Win16_InflateWithCache(input,output,packedSize,unpackedSize,alloc,crc,usePagedHistory());
}
