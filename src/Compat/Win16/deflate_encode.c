/* Native segmented Deflate encoder. Reuses the shared low-memory match finder.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include "deflate_encode.h"
#include "../../C/LzFind.h"
typedef struct {
 ISeqOutStreamPtr output;Byte *buffer;unsigned used,count,bits,extended;SRes error;
} DeflateBits;
static void deflateFlush(DeflateBits *b){
 if(b->error!=SZ_OK)return;
 if(b->used&&ISeqOutStream_Write(b->output,b->buffer,b->used)!=b->used)b->error=SZ_ERROR_WRITE;
 b->used=0;
}
static void deflateBits(DeflateBits *b,unsigned value,unsigned count){
 if(b->error!=SZ_OK)return;
 while(count--){
  b->bits|=(value&1)<<b->count;value>>=1;
  if(++b->count==8){b->buffer[b->used++]=(Byte)b->bits;b->bits=b->count=0;if(b->used==4096)deflateFlush(b);}
 }
}
static unsigned deflateReverse(unsigned value,unsigned count){unsigned result=0;while(count--){result=(result<<1)|(value&1);value>>=1;}return result;}
static void deflateSymbol(DeflateBits *b,unsigned symbol){
 unsigned code,count;
 if(symbol<144){code=0x30+symbol;count=8;}
 else if(symbol<256){code=0x190+symbol-144;count=9;}
 else if(symbol<280){code=symbol-256;count=7;}
 else{code=0xc0+symbol-280;count=8;}
 deflateBits(b,deflateReverse(code,count),count);
}
static const unsigned lengthBase[29]={3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
static const Byte lengthExtra[29]={0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
static const unsigned distanceBase[26]={1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145};
static const Byte distanceExtra[26]={0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11};
static void deflateMatch(DeflateBits *b,unsigned length,unsigned distance){
 unsigned i=0;
 while(i<(b->extended?27:28)&&length>=lengthBase[i+1])i++;
 /* Deflate64 reserves symbol 285 for long lengths; 284 also covers 258. */
 deflateSymbol(b,257+i);deflateBits(b,length-lengthBase[i],lengthExtra[i]);
 i=0;while(i<25&&distance>=distanceBase[i+1])i++;
 deflateBits(b,deflateReverse(i,5),5);deflateBits(b,distance-distanceBase[i],distanceExtra[i]);
}
static SRes deflateEncode(ISeqInStreamPtr input,ISeqOutStreamPtr output,ISzAllocPtr allocator,int extended){
 CMatchFinder *finder;IMatchFinder2 methods;DeflateBits bits;UInt32 *matches=NULL;SRes result=SZ_OK;
 memset(&bits,0,sizeof(bits));bits.output=output;bits.extended=extended;
 finder=(CMatchFinder *)ISzAlloc_Alloc(allocator,sizeof(CMatchFinder));if(!finder)return SZ_ERROR_MEM;
 MatchFinder_Construct(finder);finder->stream=input;finder->cutValue=32;
 matches=(UInt32 *)ISzAlloc_Alloc(allocator,2*258*sizeof(UInt32));bits.buffer=(Byte *)ISzAlloc_Alloc(allocator,4096);
 if(!matches||!bits.buffer||!MatchFinder_Create(finder,8192,0,258,258,allocator)){result=SZ_ERROR_MEM;goto done;}
 MatchFinder_CreateVTable(finder,&methods);methods.Init(finder);
 deflateBits(&bits,3,3); /* One final block, fixed Huffman coding. */
 while(methods.GetNumAvailableBytes(finder)&&finder->result==SZ_OK&&bits.error==SZ_OK){
  unsigned literal=*methods.GetPointerToCurrentPos(finder),length=0,distance=0;UInt32 *end=methods.GetMatches(finder,matches);
  if(end!=matches){length=(unsigned)end[-2];distance=(unsigned)end[-1]+1;}
  if(length>=3){deflateMatch(&bits,length,distance);methods.Skip(finder,length-1);}
  else deflateSymbol(&bits,literal);
 }
 if(finder->result!=SZ_OK){result=finder->result;goto done;}
 deflateSymbol(&bits,256);if(bits.count)deflateBits(&bits,0,8-bits.count);deflateFlush(&bits);result=bits.error;
 done:
 MatchFinder_Free(finder,allocator);ISzAlloc_Free(allocator,matches);ISzAlloc_Free(allocator,bits.buffer);ISzAlloc_Free(allocator,finder);return result;
}

SRes Z7Win16_DeflateEncode(ISeqInStreamPtr input,ISeqOutStreamPtr output,ISzAllocPtr allocator){return deflateEncode(input,output,allocator,0);}
SRes Z7Win16_Deflate64Encode(ISeqInStreamPtr input,ISeqOutStreamPtr output,ISzAllocPtr allocator){return deflateEncode(input,output,allocator,1);}
