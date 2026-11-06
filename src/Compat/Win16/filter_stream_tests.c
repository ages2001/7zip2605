/* Streaming filter failures, allocation ownership and short-read boundaries.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stream_folder.h"
#include "../../C/7zCrc.h"
static int allocationBudget=-1;
static unsigned live,allocations;
typedef struct { ISeekInStream iface; FILE *file; unsigned limit; int failRead; UInt32 readBytes; } Input;
typedef struct { ISeqOutStream iface; UInt32 bytes,crc; int failWrite; } Output;
static void *allocate(ISzAllocPtr p,size_t size)
{
  void *value;(void)p;
  if(allocationBudget==0)return NULL;
  if(allocationBudget>0)--allocationBudget;
  value=malloc(size);if(value){++live;++allocations;}return value;
}
static void release(ISzAllocPtr p,void *value){(void)p;if(value){--live;free(value);}}
static SRes inputRead(ISeekInStreamPtr stream,void *data,size_t *size)
{
  Input *input=(Input *)stream;
  if(input->failRead&&input->readBytes>=128){*size=0;return input->failRead==1?SZ_ERROR_READ:SZ_OK;}
  if(input->limit&&*size>input->limit)*size=input->limit;
  *size=fread(data,1,*size,input->file);input->readBytes+=(UInt32)*size;
  return ferror(input->file)?SZ_ERROR_READ:SZ_OK;
}
static SRes inputSeek(ISeekInStreamPtr stream,Int64 *position,ESzSeek origin)
{
  Input *input=(Input *)stream;
  if((Int64)(long)*position!=*position||fseek(input->file,(long)*position,origin))return SZ_ERROR_READ;
  *position=ftell(input->file);return *position<0?SZ_ERROR_READ:SZ_OK;
}
static size_t outputWrite(ISeqOutStreamPtr stream,const void *data,size_t size)
{
  Output *output=(Output *)stream;
  if(output->failWrite&&output->bytes>=12000)return 0;
  output->bytes+=(UInt32)size;output->crc=CrcUpdate(output->crc,data,size);return size;
}
static int cancelPreparation(UInt64 bytes){return bytes<12000;}
static int check(const char *path)
{
  Input input;Output output;CLookToRead2 look;CSzArEx archive;ISzAlloc alloc={allocate,release};
  Byte buffer[4096],defined=0x80;UInt32 wrongCrc=0;CSzBitUi32s savedCrc;
  SRes result,expected;unsigned baseline,needed=0;int test,valid=1;
  input.iface.Read=inputRead;input.iface.Seek=inputSeek;input.limit=0;input.failRead=0;input.readBytes=0;
  input.file=fopen(path,"rb");if(!input.file)return 0;
  LookToRead2_CreateVTable(&look,False);look.realStream=&input.iface;look.buf=buffer;look.bufSize=sizeof(buffer);LookToRead2_INIT(&look);
  SzArEx_Init(&archive);result=SzArEx_Open(&archive,&look.vt,&alloc,&alloc);
  if(result!=SZ_OK||archive.db.NumFolders!=1||archive.NumFiles!=1){fclose(input.file);SzArEx_Free(&archive,&alloc);return 0;}
  baseline=live;savedCrc=archive.db.FolderCRCs;
  if(archive.db.NumPackStreams==4){unsigned stream;
    printf("BCJ2 packed stream sizes:");for(stream=0;stream<4;stream++)printf(" %lu",(unsigned long)(archive.db.PackPositions[stream+1]-archive.db.PackPositions[stream]));puts("");
  }
  for(test=0;test<6;test++)
  {
    input.limit=13;input.failRead=test==2?1:test==3?2:0;input.readBytes=0;
    output.iface.Write=outputWrite;output.bytes=0;output.crc=CRC_INIT_VAL;output.failWrite=test==1;
    allocations=0;
    if(test==4){archive.db.FolderCRCs.Defs=&defined;archive.db.FolderCRCs.Vals=&wrongCrc;}
    if(test==5)++archive.db.CoderUnpackSizes[archive.db.FoToCoderUnpackSizes[0]];
    result=Z7Win16_DecodeFolder(&archive,0,&input.iface,&output.iface,&alloc);
    if(test==5)--archive.db.CoderUnpackSizes[archive.db.FoToCoderUnpackSizes[0]];
    archive.db.FolderCRCs=savedCrc;
    expected=test==0?SZ_OK:test==1?SZ_ERROR_WRITE:test==2?SZ_ERROR_READ:test==3?SZ_ERROR_INPUT_EOF:test==4?SZ_ERROR_CRC:SZ_ERROR_DATA;
    if(result!=expected||live!=baseline)valid=0;
    if(test==0||test==4){if(output.bytes!=SzArEx_GetFileSize(&archive,0)||CRC_GET_DIGEST(output.crc)!=archive.CRCs.Vals[0])valid=0;}
    if(!test)needed=allocations;
    printf("%s %s fault=%d result=%d bytes=%lu live=%u\n",valid?"PASS":"FAIL",path,test,result,(unsigned long)output.bytes,live);
    if(!valid)break;
  }
  for(test=0;valid&&test<(int)needed;test++)
  {
    input.failRead=0;input.readBytes=0;output.bytes=0;output.crc=CRC_INIT_VAL;output.failWrite=0;
    allocationBudget=test;
    result=Z7Win16_DecodeFolder(&archive,0,&input.iface,&output.iface,&alloc);
    allocationBudget=-1;
    valid=result==SZ_ERROR_MEM&&live==baseline&&!output.bytes;
    printf("%s %s allocation=%d result=%d live=%u\n",valid?"PASS":"FAIL",path,test,result,live);
  }
  if(valid && archive.db.NumPackStreams==4){
    input.failRead=0;input.readBytes=0;output.bytes=0;output.failWrite=0;
    result=Z7Win16_DecodeFolderProgress(&archive,0,&input.iface,&output.iface,&alloc,cancelPreparation);
    valid=result==SZ_ERROR_PROGRESS&&live==baseline&&!output.bytes;
    printf("%s BCJ2 preparation cancellation result=%d bytes=%lu live=%u\n",valid?"PASS":"FAIL",result,(unsigned long)output.bytes,live);
  }
  SzArEx_Free(&archive,&alloc);fclose(input.file);return valid&&!live;
}
int main(int argc,char **argv)
{
  int i;if(argc<2)return 7;CrcGenerateTable();
  for(i=1;i<argc;i++)if(!check(argv[i]))return 2;
  puts("PASS ALL STREAM FILTER FAILURES");return 0;
}
