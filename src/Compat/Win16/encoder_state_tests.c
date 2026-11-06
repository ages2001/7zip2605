/* Streaming/LZMA2 preparation and reuse tests for the native encoder.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../C/LzmaEnc.c"
#include "../../C/LzmaDec.h"
typedef struct {ISeqInStream iface;const Byte *data;size_t size,pos;} StateInput;
typedef struct {ISeqOutStream iface;Byte *data;size_t size,pos;} StateOutput;
static unsigned calls,failAt,live,checks;
static void *checkedAlloc(ISzAllocPtr unused,size_t bytes){void *p;(void)unused;if(++calls==failAt)return NULL;p=malloc(bytes?bytes:1);if(p)live++;return p;}
static void checkedFree(ISzAllocPtr unused,void *p){(void)unused;if(p){live--;free(p);}}
static ISzAlloc checked={checkedAlloc,checkedFree};
static void *plainAlloc(ISzAllocPtr unused,size_t bytes){(void)unused;return malloc(bytes?bytes:1);}
static void plainFree(ISzAllocPtr unused,void *p){(void)unused;free(p);}
static ISzAlloc plain={plainAlloc,plainFree};
static SRes stateRead(ISeqInStreamPtr v,void *out,size_t *size){StateInput *s=(StateInput *)v;if(*size>13)*size=13;if(*size>s->size-s->pos)*size=s->size-s->pos;memcpy(out,s->data+s->pos,*size);s->pos+=*size;return SZ_OK;}
static size_t stateWrite(ISeqOutStreamPtr v,const void *data,size_t size){StateOutput *s=(StateOutput *)v;if(size>s->size-s->pos)return 0;memcpy(s->data+s->pos,data,size);s->pos+=size;return size;}
static SRes encodeOne(CLzmaEncHandle enc,unsigned mode,unsigned lc,const Byte *data,size_t length,Byte *packed,Byte *decoded,size_t *written){
 CLzmaEncProps props;Byte properties[5];SizeT propertySize=5,sourceSize,destSize;UInt32 unpacked=(UInt32)length;ELzmaStatus status;SRes result;
 StateInput in;StateOutput out;
 in.iface.Read=stateRead;in.data=data;in.size=length;in.pos=0;out.iface.Write=stateWrite;out.data=packed;out.size=32760;out.pos=0;
 LzmaEncProps_Init(&props);props.dictSize=8192;props.algo=0;props.numThreads=1;props.lc=(int)lc;
 result=LzmaEnc_SetProps(enc,&props);if(result!=SZ_OK)return result;
 result=LzmaEnc_WriteProperties(enc,properties,&propertySize);if(result!=SZ_OK)return result;
 if(!mode){
  result=LzmaEnc_Encode(enc,&out.iface,&in.iface,NULL,&checked,&checked);*written=out.pos;
  if(result==SZ_OK&&enc->saveState.litProbs)return SZ_ERROR_FAIL;
 }else{
  result=mode==1?LzmaEnc_PrepareForLzma2(enc,&in.iface,0,&checked,&checked):LzmaEnc_MemPrepare(enc,data,length,0,&checked,&checked);
  if(result!=SZ_OK)return result;
  LzmaEnc_SaveState(enc);*written=32760;
  result=LzmaEnc_CodeOneMemBlock(enc,False,packed,written,32760,&unpacked);
  if(result==SZ_OK&&(!unpacked||unpacked>length))return SZ_ERROR_FAIL;
  LzmaEnc_RestoreState(enc);
 }
 if(result!=SZ_OK)return result;
 sourceSize=*written;destSize=mode?(size_t)unpacked:length;
 result=LzmaDecode(decoded,&destSize,packed,&sourceSize,properties,5,LZMA_FINISH_END,&status,&plain);
 if(result!=SZ_OK||destSize!=(mode?(size_t)unpacked:length)||sourceSize!=*written||memcmp(data,decoded,destSize))return SZ_ERROR_DATA;
 checks++;return SZ_OK;
}
int main(void){
 Byte *data=(Byte *)malloc(16385),*packed=(Byte *)malloc(32760),*decoded=(Byte *)malloc(16385),*reference=(Byte *)malloc(32760);CLzmaEncHandle enc;
 unsigned mode,failure,total,lc,cycle;size_t i,written,expected;SRes result;
 setvbuf(stdout,NULL,_IONBF,0);
 if(!data||!packed||!decoded||!reference)return 2;
 for(i=0;i<16385;i++)data[i]=(Byte)((i*17+(i>>7)*3)^(i>>11));
 for(mode=0;mode<3;mode++)for(lc=2;lc<=3;lc++){
  total=0;expected=0;
  for(failure=0;failure<=total;failure++){
   calls=0;failAt=failure;enc=LzmaEnc_Create(&checked);
   result=enc?encodeOne(enc,mode,lc,data,16385,packed,decoded,&written):SZ_ERROR_MEM;
   if(enc)LzmaEnc_Destroy(enc,&checked,&checked);
   if(live||(!failure&&result!=SZ_OK)||(failure&&result!=SZ_ERROR_MEM)){printf("FAIL mode=%u lc=%u fault=%u result=%d live=%u\n",mode,lc,failure,result,live);return 2;}
   if(!failure){total=calls;expected=written;memcpy(reference,packed,written);}
   checks++;
  }
  printf("PASS mode=%u lc=%u allocation faults=%u bytes=%u\n",mode,lc,total,(unsigned)expected);
 }
 calls=failAt=0;enc=LzmaEnc_Create(&checked);if(!enc)return 2;
 for(cycle=0;cycle<12;cycle++){
  mode=cycle%3;lc=2+(cycle&1);
  result=encodeOne(enc,mode,lc,data,16385,packed,decoded,&written);
  if(result!=SZ_OK){printf("FAIL reuse cycle=%u mode=%u result=%d\n",cycle,mode,result);return 2;}
 }
 LzmaEnc_Destroy(enc,&checked,&checked);if(live)return 2;
 free(data);free(packed);free(decoded);free(reference);
 printf("PASS %u checks, streaming/LZMA2/memory preparation, state save/restore, property changes, handle reuse, allocation failures; zero live blocks\n",checks);return 0;
}
