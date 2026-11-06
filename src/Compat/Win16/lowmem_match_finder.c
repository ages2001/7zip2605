/* Low-memory hash-chain match finder for the upstream LZMA encoder.
   Every allocation fits a Win16 segment. Candidates are compared byte for
   byte; unlike the large upstream hash tables, no prefix match is assumed.
   This backend currently accepts dictionaries up to 12 KiB. */
#include <string.h>
#include "../../C/LzFind.h"
#if defined(__WATCOMC__) && defined(__WINDOWS__)
#include <windows.h>
#endif
#ifndef Z7_WIN16
#error This backend is only for the segmented Win16 target
#endif
#define LOWMEM_HASH_SIZE 1024

void MatchFinder_Construct(CMatchFinder *p){
 memset(p,0,sizeof(*p));p->cutValue=32;p->numHashBytes=3;
 p->numHashBytes_Min=2;p->expectedDataSize=(UInt64)(Int64)-1;
}
void MatchFinder_Free(CMatchFinder *p,ISzAllocPtr alloc){
 ISzAlloc_Free(alloc,p->bufBase);ISzAlloc_Free(alloc,p->hash);
 p->bufBase=NULL;p->hash=NULL;p->son=NULL;p->numRefs=0;
}
int MatchFinder_Create(CMatchFinder *p,UInt32 historySize,UInt32 keepBefore,
    UInt32 matchMaxLen,UInt32 keepAfter,ISzAllocPtr alloc){
 UInt32 refs,bytes,block,refill=4096;
 if(!historySize||historySize>12288UL||keepBefore>16384UL||keepAfter>4096UL||!matchMaxLen||matchMaxLen>273)return 0;
 p->keepSizeBefore=historySize+keepBefore+1;
 p->keepSizeAfter=keepAfter+matchMaxLen;if(p->keepSizeAfter<2)p->keepSizeAfter=2;
 /* Extra refill space batches input reads; it is not compression history or
    parser look-ahead. Keep the normal batching in protected mode and DOS.
    A smaller real-mode batch avoids a large contiguous global-heap request
    when an update has both a decoder and an encoder alive. */
#if defined(__WATCOMC__) && defined(__WINDOWS__)
 if(!(GetWinFlags()&WF_PMODE))refill=2048;
#endif
 block=p->keepSizeBefore+p->keepSizeAfter+refill;
 refs=LOWMEM_HASH_SIZE+historySize+1;bytes=refs*sizeof(CLzRef);
 /* Win16 ptrdiff_t is signed 16-bit; window pointer subtraction must fit. */
 if(block>32760UL||bytes>65535UL)return 0;
 MatchFinder_Free(p,alloc);
 p->historySize=historySize;p->cyclicBufferSize=historySize+1;
 p->matchMaxLen=matchMaxLen;p->blockSize=block;p->hashMask=LOWMEM_HASH_SIZE-1;p->fixedHashSize=0;
 p->hash=(CLzRef *)ISzAlloc_Alloc(alloc,(size_t)bytes);
 if(!p->directInput)p->bufBase=(Byte *)ISzAlloc_Alloc(alloc,(size_t)block);
 if(!p->hash||(!p->directInput&&!p->bufBase)){MatchFinder_Free(p,alloc);return 0;}
 p->son=p->hash+LOWMEM_HASH_SIZE;p->numRefs=(size_t)refs;return 1;
}
static void lowmemFill(CMatchFinder *p){
 UInt32 available,offset,keep,room;
 if(p->directInput||p->streamEndWasReached||p->result!=SZ_OK)return;
 available=p->streamPos-p->pos;offset=(UInt32)(p->buffer-p->bufBase);
 if(offset+available+p->keepSizeAfter>=p->blockSize){
  keep=offset<p->keepSizeBefore?offset:p->keepSizeBefore;
  memmove(p->bufBase,p->buffer-keep,(size_t)(keep+available));
  p->buffer=p->bufBase+keep;offset=keep;
 }
 room=p->blockSize-offset-available;
 while(room){
  size_t size=(size_t)room;SRes result=p->stream->Read(p->stream,p->bufBase+offset+available,&size);
  if(result!=SZ_OK){p->result=result;return;}
  if(size>room){p->result=SZ_ERROR_READ;return;}
  if(!size){p->streamEndWasReached=1;return;}
  available+=(UInt32)size;p->streamPos+=(UInt32)size;room-=(UInt32)size;
 }
}
void MatchFinder_Init(void *object){
 CMatchFinder *p=(CMatchFinder *)object;
 memset(p->hash,0,p->numRefs*sizeof(CLzRef));
 p->pos=p->streamPos=1;p->cyclicBufferPos=0;p->result=SZ_OK;p->streamEndWasReached=0;
 if(p->directInput){p->streamPos+=(UInt32)p->directInputRem;p->directInputRem=0;p->streamEndWasReached=1;}
 else {p->buffer=p->bufBase;lowmemFill(p);}
}
static UInt32 lowmemAvailable(void *object){CMatchFinder *p=(CMatchFinder *)object;return p->streamPos-p->pos;}
static const Byte *lowmemPointer(void *object){return ((CMatchFinder *)object)->buffer;}
static void lowmemAdvance(CMatchFinder *p){
 p->buffer++;p->pos++;if(++p->cyclicBufferPos==p->cyclicBufferSize)p->cyclicBufferPos=0;
 if(p->streamPos-p->pos<=p->keepSizeAfter)lowmemFill(p);
}
static UInt32 lowmemInsert(CMatchFinder *p){
 UInt32 hash=((UInt32)p->buffer[0]*257+p->buffer[1])&(LOWMEM_HASH_SIZE-1);
 UInt32 previous=p->hash[hash];p->son[p->cyclicBufferPos]=previous;p->hash[hash]=p->pos;return previous;
}
static UInt32 *lowmemMatches(void *object,UInt32 *distances){
 CMatchFinder *p=(CMatchFinder *)object;UInt32 available=p->streamPos-p->pos,candidate,best=1,limit,cycles;
 if(available>=2){
  candidate=lowmemInsert(p);limit=available<p->matchMaxLen?available:p->matchMaxLen;
  cycles=p->cutValue?p->cutValue:1;
  while(candidate&&candidate<p->pos&&p->pos-candidate<=p->historySize&&cycles--){
   UInt32 delta=p->pos-candidate,len=0;const Byte *other=p->buffer-delta;
   while(len<limit&&p->buffer[len]==other[len])len++;
   if(len>best){*distances++=len;*distances++=delta-1;best=len;if(len==limit)break;}
   candidate=p->son[(candidate-1)%p->cyclicBufferSize];
  }
 }
 if(available)lowmemAdvance(p);return distances;
}
static void lowmemSkip(void *object,UInt32 count){
 CMatchFinder *p=(CMatchFinder *)object;
 while(count--&&p->streamPos!=p->pos){if(p->streamPos-p->pos>=2)lowmemInsert(p);lowmemAdvance(p);}
}
void MatchFinder_CreateVTable(CMatchFinder *p,IMatchFinder2 *table){
 (void)p;table->Init=MatchFinder_Init;table->GetNumAvailableBytes=lowmemAvailable;
 table->GetPointerToCurrentPos=lowmemPointer;table->GetMatches=lowmemMatches;table->Skip=lowmemSkip;
}
