/* Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#ifdef Z7_ALLOC_FAILURE_TRACE
#include <stdio.h>
#endif
#include "stream_folder.h"
#include "deflate_stream.h"
#include "../../C/LzmaDec.h"
#include "../../C/Lzma2Dec.h"
#include "../../C/7zCrc.h"
#include "stream_filters.h"
#include "paged_dictionary.h"
#include "../../C/Bcj2.h"
#include "aes7z.h"
#if defined(__WATCOMC__) && defined(__WINDOWS__)
#include <windows.h>
static unsigned bcj2Cs(void);
#pragma aux bcj2Cs = "mov ax,cs" value [ax] modify exact [ax];
#endif
#define CHUNK_SIZE 4096
#define METHOD_DEFLATE 0x040108UL
#define METHOD_DEFLATE64 0x040109UL
#define METHOD_COPY 0
#define METHOD_LZMA 0x030101UL
#define METHOD_LZMA2 0x21
static SRes decodeLinearFolder(const CSzArEx *archive, UInt32 folderIndex,
    ISeekInStreamPtr input, ISeqOutStreamPtr output, ISzAllocPtr allocator,
    const CSzFolder *parsed, const Byte *properties)
{
  CSzFolder folder=*parsed;
  const CSzAr *db=&archive->db;
  UInt32 method,packIndex,unusedCrc,order[SZ_NUM_CODERS_IN_FOLDER_MAX];
  UInt32 i,j,current,visited,filterCount;
  const CSzCoderInfo *mainCoder;
  FilterStream filters[SZ_NUM_CODERS_IN_FOLDER_MAX-1];
  CheckedStream checked;
  ISeqOutStreamPtr decodedOutput;
  UInt64 packedLeft,outputLeft,absolute;
  Int64 position;
  Byte *packed=NULL,*decoded=NULL;
  size_t available=0,offset=0;
  CLzmaDec lzma;
  CLzma2Dec lzma2;
  SRes result;
  ELzmaStatus status=LZMA_STATUS_NOT_SPECIFIED;
  BoolInt allocated=False;
  UInt32 mainStart=0;
  const Byte *aesProps=NULL;size_t aesPropsSize=0;UInt64 aesCipherLen=0;
  Aes7zStream aesStream;ISeekInStreamPtr mainInput;
  memset(filters,0,sizeof(filters));
  filterCount=0;
  if(folderIndex>=db->NumFolders)return SZ_ERROR_PARAM;
  if(!folder.NumCoders || folder.NumCoders>SZ_NUM_CODERS_IN_FOLDER_MAX
      || folder.NumPackStreams!=1 || folder.NumBonds!=folder.NumCoders-1
      || folder.PackStreams[0]>=folder.NumCoders)
    return SZ_ERROR_UNSUPPORTED;
  /* Follow the actual bonds rather than assuming a particular coder order.
     Every stage in this helper has one input and output. BCJ2 branches are
     validated separately before using this decoder for each packed branch. */
  for(i=0;i<folder.NumCoders;i++)
    if(folder.Coders[i].NumStreams!=1)return SZ_ERROR_UNSUPPORTED;
  for(i=0;i<folder.NumBonds;i++)
    if(folder.Bonds[i].InIndex>=folder.NumCoders || folder.Bonds[i].OutIndex>=folder.NumCoders)
      return SZ_ERROR_ARCHIVE;
  current=folder.PackStreams[0];visited=0;
  for(i=0;i<folder.NumCoders;i++)
  {
    UInt32 next=folder.NumCoders;
    if(current>=folder.NumCoders || (visited&((UInt32)1<<current)))return SZ_ERROR_ARCHIVE;
    order[i]=current;visited|=(UInt32)1<<current;
    for(j=0;j<folder.NumBonds;j++)
      if(folder.Bonds[j].OutIndex==current)
      {
        if(next!=folder.NumCoders)return SZ_ERROR_ARCHIVE;
        next=folder.Bonds[j].InIndex;
      }
    if(i+1<folder.NumCoders)current=next;
    else if(next!=folder.NumCoders || current!=folder.UnpackStream
        || current!=db->FoToMainUnpackSizeIndex[folderIndex])return SZ_ERROR_ARCHIVE;
  }
  /* 7zAES, when present, is the pack-fed coder (order[0]); the real codec is
     then order[1] and its packed input is served through an AES-CBC decrypt
     wrapper. Capture the AES props from the original base before it is advanced
     to the main coder. Without AES this is byte-identical to before. */
  mainStart=0;
  if(folder.Coders[order[0]].MethodID==METHOD_AES256SHA256){
    if(folder.NumCoders<2)return SZ_ERROR_ARCHIVE;
    mainStart=1;
    aesProps=properties+folder.Coders[order[0]].PropsOffset;
    aesPropsSize=folder.Coders[order[0]].PropsSize;
  }
  mainCoder=&folder.Coders[order[mainStart]];
  method=mainCoder->MethodID;
  if(method!=METHOD_COPY && method!=METHOD_LZMA && method!=METHOD_LZMA2 && method!=METHOD_DEFLATE && method!=METHOD_DEFLATE64)
    return SZ_ERROR_UNSUPPORTED;
  if(((method==METHOD_COPY || method==METHOD_DEFLATE || method==METHOD_DEFLATE64) && mainCoder->PropsSize)
      || (method==METHOD_LZMA && mainCoder->PropsSize!=5)
      || (method==METHOD_LZMA2 && mainCoder->PropsSize!=1))
    return SZ_ERROR_DATA;
  filterCount=folder.NumCoders-1-mainStart;
  for(i=0;i<filterCount;i++)
    RINOK(filterProperties(&filters[i],&folder.Coders[order[mainStart+1+i]],properties))
  properties+=mainCoder->PropsOffset;
  packIndex=db->FoStartPackStreamIndex[folderIndex];
  if(packIndex>=db->NumPackStreams)return SZ_ERROR_ARCHIVE;
  packedLeft=db->PackPositions[packIndex+1]-db->PackPositions[packIndex];
  outputLeft=SzAr_GetFolderUnpackSize(db,folderIndex);
  if(db->FoToCoderUnpackSizes[folderIndex+1]-db->FoToCoderUnpackSizes[folderIndex]!=folder.NumCoders)
    return SZ_ERROR_ARCHIVE;
  /* Every stage outputs the final unpacked size, except a 7zAES stage whose
     (decrypted) output is the still-compressed stream feeding the main codec. */
  for(i=0;i<folder.NumCoders;i++)
    if(!(mainStart && i==order[0]))
      if(db->CoderUnpackSizes[db->FoToCoderUnpackSizes[folderIndex]+i]!=outputLeft)
        return SZ_ERROR_DATA;
  /* With AES the packed stream is the ciphertext (block-padded); the main codec
     must be fed the AES coder's real output size (the compressed stream length),
     not the padded ciphertext length. The wrapper still decrypts the full
     ciphertext; the loop just stops at the meaningful byte count. */
  if(mainStart){
    aesCipherLen=packedLeft;
    packedLeft=db->CoderUnpackSizes[db->FoToCoderUnpackSizes[folderIndex]+order[0]];
    if(packedLeft>aesCipherLen)return SZ_ERROR_DATA;
  }
  if(method==METHOD_COPY && packedLeft!=outputLeft)return SZ_ERROR_DATA;
  absolute=archive->dataPos+db->PackPositions[packIndex];
  if(absolute<archive->dataPos)return SZ_ERROR_ARCHIVE;
  position=(Int64)absolute;
  if(position<0 || (UInt64)position!=absolute)return SZ_ERROR_UNSUPPORTED;
  RINOK(ISeekInStream_Seek(input,&position,SZ_SEEK_SET))
  mainInput=input;
  if(mainStart){
    RINOK(aes7zInit(&aesStream,input,aesProps,aesPropsSize,aesCipherLen))
    mainInput=&aesStream.vt;
  }
  checked.iface.Write=checkedWrite;checked.next=output;checked.crc=CRC_INIT_VAL;
  decodedOutput=&checked.iface;
  /* Reserve the larger probability block before small I/O/filter buffers.
     Real-mode Windows cannot satisfy a contiguous allocation merely because
     the total free space is sufficient. The decoder's working sizes and
     protected-mode buffering remain unchanged. */
  if(method==METHOD_LZMA){
    LzmaDec_Construct(&lzma);allocated=True;
    result=LzmaDec_Allocate(&lzma,properties,5,allocator);
    if(result!=SZ_OK)goto finish;
    LzmaDec_Init(&lzma);
  }else if(method==METHOD_LZMA2){
    Lzma2Dec_Construct(&lzma2);allocated=True;
    result=Lzma2Dec_Allocate(&lzma2,properties[0],allocator);
    if(result!=SZ_OK)goto finish;
    Lzma2Dec_Init(&lzma2);
  }
  for(i=filterCount;i>0;i--)
  {
    FilterStream *filter=&filters[i-1];
    filter->iface.Write=filterWrite;filter->next=decodedOutput;
    filter->buffer=(Byte *)ISzAlloc_Alloc(allocator,Z7_FILTER_BUFFER);
    if(!filter->buffer){result=SZ_ERROR_MEM;goto finish;}
    decodedOutput=&filter->iface;
  }
  if(method==METHOD_DEFLATE || method==METHOD_DEFLATE64){
    result=method==METHOD_DEFLATE?Z7Win16_Inflate(mainInput,decodedOutput,packedLeft,outputLeft,allocator,&unusedCrc):Z7Win16_Inflate64(mainInput,decodedOutput,packedLeft,outputLeft,allocator,&unusedCrc);
    goto decoded;
  }
  packed=(Byte *)ISzAlloc_Alloc(allocator,CHUNK_SIZE);
  decoded=(Byte *)ISzAlloc_Alloc(allocator,CHUNK_SIZE);
  if(!packed||!decoded){result=SZ_ERROR_MEM;goto finish;}
  for(;;){
    SizeT inSize,outSize;
    ELzmaFinishMode mode;
    if(offset==available && packedLeft){
      size_t n=packedLeft>CHUNK_SIZE?CHUNK_SIZE:(size_t)packedLeft;
      result=ISeekInStream_Read(mainInput,packed,&n);
      if(result!=SZ_OK)break;
      if(!n){result=SZ_ERROR_INPUT_EOF;break;}
      packedLeft-=n;available=n;offset=0;
    }
    inSize=available-offset;
    outSize=outputLeft>CHUNK_SIZE?CHUNK_SIZE:(SizeT)outputLeft;
    mode=outputLeft<=CHUNK_SIZE?LZMA_FINISH_END:LZMA_FINISH_ANY;
    if(method==METHOD_COPY){
      if(outSize>inSize)outSize=inSize;
      inSize=outSize;memcpy(decoded,packed+offset,outSize);result=SZ_OK;
    }else if(method==METHOD_LZMA)
      result=LzmaDec_DecodeToBuf(&lzma,decoded,&outSize,packed+offset,&inSize,mode,&status);
    else
      result=Lzma2Dec_DecodeToBuf(&lzma2,decoded,&outSize,packed+offset,&inSize,mode,&status);
    offset+=inSize;outputLeft-=outSize;
    if(result!=SZ_OK)break;
    if(outSize && ISeqOutStream_Write(decodedOutput,decoded,outSize)!=outSize){result=SZ_ERROR_WRITE;break;}
    if(!outputLeft && !packedLeft && offset==available){
      if(method!=METHOD_COPY && status!=LZMA_STATUS_FINISHED_WITH_MARK
          && !(method==METHOD_LZMA && status==LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK))
        result=SZ_ERROR_DATA;
      break;
    }
    if(status==LZMA_STATUS_FINISHED_WITH_MARK || (!outSize&&!inSize)){
      result=SZ_ERROR_DATA;break;
    }
  }
decoded:
  for(i=0;result==SZ_OK&&i<filterCount;i++)result=filterFinish(&filters[i]);
  if(result==SZ_OK && SzBitWithVals_Check(&db->FolderCRCs,folderIndex)
      && CRC_GET_DIGEST(checked.crc)!=db->FolderCRCs.Vals[folderIndex])result=SZ_ERROR_CRC;
finish:
  if(allocated){
    if(method==METHOD_LZMA)LzmaDec_Free(&lzma,allocator);
    else Lzma2Dec_Free(&lzma2,allocator);
#ifdef Z7_WIN16_PAGED_DICT
    if(result==SZ_OK)result=method==METHOD_LZMA?LzmaDec_DictionaryStatus(&lzma):LzmaDec_DictionaryStatus(&lzma2.decoder);
#endif
  }
  ISzAlloc_Free(allocator,packed);ISzAlloc_Free(allocator,decoded);
  for(i=0;i<filterCount;i++)ISzAlloc_Free(allocator,filters[i].buffer);
  return result;
}

#if defined(__WATCOMC__) && defined(__WINDOWS__)
#pragma code_seg("BCJ2_STREAM_TEXT")
#endif
#define METHOD_BCJ2 0x0303011BUL
#define BCJ2_CHUNK 1024
/* Decode branches sequentially into the existing checked, private disk-backed
   store. Only one compression decoder is resident at a time. */
typedef struct {
  ISeqOutStream iface;
  Z7Win16Dictionary *store;
  UInt32 position,limit;
  SRes error;
  Z7Win16DecodeProgress progress;
} Bcj2StoreOutput;
static size_t bcj2StoreWrite(ISeqOutStreamPtr stream,const void *data,size_t size)
{
  Bcj2StoreOutput *p=(Bcj2StoreOutput *)stream;
  if(p->progress && !p->progress(p->position)){p->error=SZ_ERROR_PROGRESS;return 0;}
  if(size>p->limit-p->position){p->error=SZ_ERROR_DATA;return 0;}
  p->error=Z7Win16_DictionaryWrite(p->store,p->position,(const Byte *)data,size);
  if(p->error)return 0;p->position+=(UInt32)size;return size;
}
static SRes decodeBcj2Folder(const CSzArEx *archive,UInt32 folderIndex,
    ISeekInStreamPtr input,ISeqOutStreamPtr output,ISzAllocPtr allocator,
    const CSzFolder *folder,const Byte *properties,unsigned bcj,Z7Win16DecodeProgress progress)
{
  const CSzAr *db=&archive->db;
  unsigned starts[4],coders[4],packs[4],i,j,k,streams=0,used=0;
  unsigned chunk=BCJ2_CHUNK,pages=2;
  UInt32 begin[4],left[4],total=0,packIndex,unpackIndex,absolutePack;
  UInt64 sizes[4],outLeft;
  Z7Win16Dictionary *store=NULL;
  CBcj2Dec *decoder=NULL;
  Byte *buffers=NULL,*decoded;
  CheckedStream checked;
  SRes result=SZ_OK,cleanup;
#if defined(__WATCOMC__) && defined(__WINDOWS__)
  HGLOBAL lockedCode;
#endif
  if(folder->NumCoders>4 || folder->NumPackStreams!=4 ||
      folder->NumBonds!=folder->NumCoders-1 || folder->UnpackStream!=bcj ||
      db->FoToMainUnpackSizeIndex[folderIndex]!=bcj)return SZ_ERROR_ARCHIVE;
  if(folder->Coders[bcj].PropsSize)return SZ_ERROR_UNSUPPORTED;
  unpackIndex=db->FoToCoderUnpackSizes[folderIndex];
  if(db->FoToCoderUnpackSizes[folderIndex+1]-unpackIndex!=folder->NumCoders)return SZ_ERROR_ARCHIVE;
  packIndex=db->FoStartPackStreamIndex[folderIndex];
  if(packIndex>db->NumPackStreams || db->NumPackStreams-packIndex<4)return SZ_ERROR_ARCHIVE;
  for(i=0;i<folder->NumCoders;i++){
    starts[i]=streams;
    if(folder->Coders[i].NumStreams!=(i==bcj?4:1))return SZ_ERROR_UNSUPPORTED;
    streams+=(i==bcj?4:1);
  }
  for(i=0;i<4;i++)coders[i]=packs[i]=4;
  /* Each non-BCJ2 output must feed exactly one BCJ2 input. */
  for(i=0;i<folder->NumBonds;i++){
    UInt32 in=folder->Bonds[i].InIndex,out=folder->Bonds[i].OutIndex;
    if(out>=folder->NumCoders || out==bcj || in<starts[bcj] || in>=starts[bcj]+4)
      return SZ_ERROR_UNSUPPORTED;
    j=(unsigned)(in-starts[bcj]);
    if(coders[j]!=4)return SZ_ERROR_ARCHIVE;
    for(k=0;k<4;k++)if(coders[k]==out)return SZ_ERROR_ARCHIVE;
    coders[j]=(unsigned)out;
  }
  outLeft=db->CoderUnpackSizes[unpackIndex+bcj];
  for(i=0;i<4;i++){
    unsigned wanted=coders[i]==4?starts[bcj]+i:starts[coders[i]];
    for(k=0;k<4;k++)if(folder->PackStreams[k]==wanted){
      if(packs[i]!=4 || (used&(1U<<k)))return SZ_ERROR_ARCHIVE;
      packs[i]=k;used|=1U<<k;
    }
    if(packs[i]==4)return SZ_ERROR_ARCHIVE;
    absolutePack=packIndex+packs[i];
    if(db->PackPositions[absolutePack+1]<db->PackPositions[absolutePack])return SZ_ERROR_ARCHIVE;
    sizes[i]=coders[i]==4?db->PackPositions[absolutePack+1]-db->PackPositions[absolutePack]:
        db->CoderUnpackSizes[unpackIndex+coders[i]];
    /* The backing store uses checked signed DOS file offsets. */
    if(sizes[i]>(UInt64)0x7FFFF000UL-total)return SZ_ERROR_UNSUPPORTED;
    begin[i]=total;left[i]=(UInt32)sizes[i];total+=left[i];
  }
  if(used!=15 || outLeft!=sizes[0]+sizes[1]+sizes[2] ||
      (sizes[1]&3) || (sizes[2]&3) || sizes[3]<5)return SZ_ERROR_DATA;
  /* A progress callback can pump Windows messages during branch preparation.
     Pin this caller while active, then allow real mode to reclaim its code
     before a later encoder operation needs conventional memory. */
#if defined(__WATCOMC__) && defined(__WINDOWS__)
  lockedCode=LockSegment(bcj2Cs());if(!lockedCode)return SZ_ERROR_MEM;
  if(!(GetWinFlags()&WF_PMODE)){chunk=512;pages=1;}
#elif defined(Z7_WIN16_SMALL_BCJ2)
  chunk=512;pages=1;
#endif
  result=Z7Win16_DictionaryCreateWithPages(&store,total,allocator,pages);if(result)goto done;
  for(i=0;i<4;i++){
    CSzArEx sub=*archive;
    CSzFolder single;
    UInt32 firstPack[2],sizeOffsets[2]={0,1};Byte mainIndex=0;
    Bcj2StoreOutput target;
    memset(&single,0,sizeof(single));single.NumCoders=single.NumPackStreams=1;
    if(coders[i]!=4)single.Coders[0]=folder->Coders[coders[i]];
    else single.Coders[0].NumStreams=1; /* raw branch: Copy */
    firstPack[0]=packIndex+packs[i];firstPack[1]=firstPack[0]+1;
    sub.db.NumFolders=1;sub.db.FoStartPackStreamIndex=firstPack;
    sub.db.FoToCoderUnpackSizes=sizeOffsets;sub.db.FoToMainUnpackSizeIndex=&mainIndex;
    sub.db.CoderUnpackSizes=&sizes[i];sub.db.FolderCRCs.Defs=NULL;
    target.iface.Write=bcj2StoreWrite;target.store=store;target.position=begin[i];
    target.limit=begin[i]+left[i];target.error=SZ_OK;target.progress=progress;
    result=decodeLinearFolder(&sub,0,input,&target.iface,allocator,&single,properties);
    if(target.error)result=target.error;
    if(result)goto done;
    if(target.position!=target.limit){result=SZ_ERROR_DATA;goto done;}
  }
  /* Branch preparation owns a compression decoder and its I/O buffers.
     Allocate the merge-only state after those have been released, so the
     segmented heap need not hold both phases' working sets at once. */
  buffers=(Byte *)ISzAlloc_Alloc(allocator,5*chunk);
  decoder=(CBcj2Dec *)ISzAlloc_Alloc(allocator,sizeof(*decoder));
  if(!buffers||!decoder){result=SZ_ERROR_MEM;goto done;}
  Bcj2Dec_Init(decoder);decoded=buffers+4*chunk;
  for(i=0;i<4;i++)decoder->bufs[i]=decoder->lims[i]=buffers+i*chunk;
  checked.iface.Write=checkedWrite;checked.next=output;checked.crc=CRC_INIT_VAL;
  for(;;){
    SizeT produced;unsigned state=decoder->state;
    if(state<4 && decoder->bufs[state]==decoder->lims[state] && left[state]){
      SizeT n=left[state]>chunk?chunk:(SizeT)left[state];
      result=Z7Win16_DictionaryRead(store,begin[state],buffers+state*chunk,n);
      if(result)goto done;
      begin[state]+=n;left[state]-=n;decoder->bufs[state]=buffers+state*chunk;
      decoder->lims[state]=decoder->bufs[state]+n;
    }
    decoder->dest=decoded;decoder->destLim=decoded+(outLeft>chunk?chunk:(SizeT)outLeft);
    result=Bcj2Dec_Decode(decoder);if(result)goto done;
    produced=(SizeT)(decoder->dest-decoded);outLeft-=produced;
    if(produced && checkedWrite(&checked.iface,decoded,produced)!=produced){result=SZ_ERROR_WRITE;goto done;}
    if(!outLeft){
      if(!Bcj2Dec_IsMaybeFinished(decoder)){result=SZ_ERROR_DATA;goto done;}
      for(i=0;i<4;i++)if(left[i] || decoder->bufs[i]!=decoder->lims[i]){result=SZ_ERROR_DATA;goto done;}
      break;
    }
    if(!produced && (decoder->state>=4 || !left[decoder->state])){result=SZ_ERROR_DATA;goto done;}
  }
  if(SzBitWithVals_Check(&db->FolderCRCs,folderIndex) &&
      CRC_GET_DIGEST(checked.crc)!=db->FolderCRCs.Vals[folderIndex])result=SZ_ERROR_CRC;
done:
  ISzAlloc_Free(allocator,decoder);ISzAlloc_Free(allocator,buffers);
  cleanup=Z7Win16_DictionaryDestroy(store,allocator);if(result==SZ_OK)result=cleanup;
#if defined(__WATCOMC__) && defined(__WINDOWS__)
  UnlockSegment((UINT)lockedCode);
#ifdef Z7_ALLOC_FAILURE_TRACE
  {FILE *trace=fopen("C:\\MEM66.TXT","a");if(trace){
    setvbuf(trace,NULL,_IONBF,0);
    fprintf(trace,"BCJ2 result=%d handle=%04X flags=%04X free=%lu\n",result,lockedCode,GlobalFlags(lockedCode),GetFreeSpace(0));fclose(trace);
  }}
#endif
#endif
  return result;
}
#if defined(__WATCOMC__) && defined(__WINDOWS__)
#pragma code_seg()
typedef struct {
  ISeqOutStream iface;
  ISeqOutStreamPtr target;
  BoolInt touched;
} Bcj2RetryOutput;
static size_t bcj2RetryWrite(ISeqOutStreamPtr stream,const void *data,size_t size)
{
  Bcj2RetryOutput *p=(Bcj2RetryOutput *)stream;
  if(size)p->touched=True;
  return ISeqOutStream_Write(p->target,data,size);
}
#endif
SRes Z7Win16_DecodeFolderProgress(const CSzArEx *archive,UInt32 folderIndex,
    ISeekInStreamPtr input,ISeqOutStreamPtr output,ISzAllocPtr allocator,Z7Win16DecodeProgress progress)
{
  CSzFolder folder;CSzData description;const Byte *properties;unsigned i;
  if(folderIndex>=archive->db.NumFolders)return SZ_ERROR_PARAM;
  description.Data=archive->db.CodersData+archive->db.FoCodersOffsets[folderIndex];
  description.Size=archive->db.FoCodersOffsets[folderIndex+1]-archive->db.FoCodersOffsets[folderIndex];
  properties=description.Data;RINOK(SzGetNextFolderItem(&folder,&description))
  if(description.Size || !folder.NumCoders)return SZ_ERROR_ARCHIVE;
  for(i=0;i<folder.NumCoders;i++)if(folder.Coders[i].MethodID==METHOD_BCJ2){
#if defined(__WATCOMC__) && defined(__WINDOWS__)
    if(!(GetWinFlags()&WF_PMODE)){
      Bcj2RetryOutput retry;SRes result;
      retry.iface.Write=bcj2RetryWrite;retry.target=output;retry.touched=False;
      result=decodeBcj2Folder(archive,folderIndex,input,&retry.iface,allocator,&folder,properties,i,progress);
      /* Real-mode compaction can retain active decoder/code frames even when
         their allocations cannot be satisfied. Unwind that entire phase
         before compacting and retrying once. Never replay visible output or
         a cancellation; genuine memory exhaustion remains an error. */
      if(result==SZ_ERROR_MEM&&!retry.touched){
        if(progress&&!progress(0))return SZ_ERROR_PROGRESS;
        GlobalCompact(0xFFFFFFFFUL);
        result=decodeBcj2Folder(archive,folderIndex,input,&retry.iface,allocator,&folder,properties,i,progress);
      }
      return result;
    }
#endif
    return decodeBcj2Folder(archive,folderIndex,input,output,allocator,&folder,properties,i,progress);
  }
  return decodeLinearFolder(archive,folderIndex,input,output,allocator,&folder,properties);
}

SRes Z7Win16_DecodeFolder(const CSzArEx *archive,UInt32 folderIndex,
    ISeekInStreamPtr input,ISeqOutStreamPtr output,ISzAllocPtr allocator)
{
  return Z7Win16_DecodeFolderProgress(archive,folderIndex,input,output,allocator,NULL);
}
