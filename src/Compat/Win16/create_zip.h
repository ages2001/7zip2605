/* Streaming classic ZIP creation; existing paths are never overwritten.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_CREATE_ZIP_H
#define Z7_WIN16_CREATE_ZIP_H
#include "deflate_encode.h"
static char zipCreateMessage[320];
typedef struct {
 const char *name;Byte directory;UInt32 size,packed,crc,offset,mtime,attributes;unsigned date,time;
} ZipCreateEntry;
static SRes zipCreateHeader(FILE *out,const ZipCreateEntry *e,unsigned method,int central){
 Byte data[64];CreateHeader h;unsigned n=(unsigned)strlen(e->name);
 h.data=data;h.used=0;h.failed=0;createLE(&h,central?0x02014b50UL:0x04034b50UL,4);
 if(central)createLE(&h,20,2);createLE(&h,method==9?21:20,2);createLE(&h,0,2);createLE(&h,method,2);
 createLE(&h,e->time,2);createLE(&h,e->date,2);createLE(&h,e->crc,4);createLE(&h,e->packed,4);createLE(&h,e->size,4);
 createLE(&h,n+e->directory,2);createLE(&h,9,2);
 if(central){createLE(&h,0,2);createLE(&h,0,2);createLE(&h,0,2);createLE(&h,e->attributes,4);createLE(&h,e->offset,4);}
 if(!createWriteBytes(out,data,h.used)||!createWriteBytes(out,e->name,n))return SZ_ERROR_WRITE;
 if(e->directory&&!createWriteBytes(out,"/",1))return SZ_ERROR_WRITE;
 h.used=0;createLE(&h,0x5455,2);createLE(&h,5,2);createByte(&h,1);createLE(&h,e->mtime,4);
 return createWriteBytes(out,data,h.used)?SZ_OK:SZ_ERROR_WRITE;
}
static SRes createZipTree(const char *path,const InputTree *tree,ISzAllocPtr alloc,int deflate){
 unsigned count=tree->count,method=deflate==2?9:deflate?8:0;
 ZipCreateEntry *entries=NULL;FILE *out=NULL,*source=NULL;Byte *buffer=NULL;unsigned i;int owned=0,fd;
 UInt64 processed=0;SRes result=SZ_OK;long position,start,central,end;struct stat st;
 zipCreateMessage[0]=0;
 if(deflate<0||deflate>2||!count||count>4096)return SZ_ERROR_UNSUPPORTED;
#ifdef __WATCOMC__
 if(!validDosDestination(path))return SZ_ERROR_UNSUPPORTED;
#endif
 entries=(ZipCreateEntry *)ISzAlloc_Alloc(alloc,count*sizeof(ZipCreateEntry));
 buffer=(Byte *)ISzAlloc_Alloc(alloc,4096);if(!entries||!buffer){result=SZ_ERROR_MEM;goto done;}memset(entries,0,count*sizeof(ZipCreateEntry));
 for(i=0;i<count;i++){
  ZipCreateEntry *e=entries+i;const InputNode *node=tree->items+i;struct tm *date;
  e->name=node->name;e->directory=node->directory;
  if(stat(node->source,&st)){result=SZ_ERROR_READ;goto done;}
  if((st.st_mode&S_IFMT)!=(node->directory?S_IFDIR:S_IFREG)||st.st_size<0||(UInt64)st.st_size>0x7fffffffUL){result=SZ_ERROR_UNSUPPORTED;goto done;}
  date=localtime(&st.st_mtime);
  if(!date||date->tm_year<80||date->tm_year>207||(Int64)st.st_mtime<0||(UInt64)st.st_mtime>0x7fffffffUL){
   strcpy(zipCreateMessage,"ERROR: This file timestamp cannot be represented by the current ZIP writer.");result=SZ_ERROR_UNSUPPORTED;goto done;
  }
  e->size=node->directory?0:(UInt32)st.st_size;e->mtime=(UInt32)st.st_mtime;e->date=((date->tm_year-80)<<9)|((date->tm_mon+1)<<5)|date->tm_mday;
  e->time=(date->tm_hour<<11)|(date->tm_min<<5)|(date->tm_sec/2);e->attributes=(node->directory?0x10:0x20)|((st.st_mode&S_IWUSR)?0:1);
#ifdef __WATCOMC__
  {unsigned attributes;if(_dos_getfileattr(node->source,&attributes)){result=SZ_ERROR_READ;goto done;}e->attributes=attributes;}
#endif
 }
 if(testProgress&&!testProgress(0)){result=SZ_ERROR_PROGRESS;goto done;}
 fd=open(path,O_RDWR|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);
 if(fd<0){strcpy(zipCreateMessage,"ERROR: Cannot create a new ZIP archive. Existing files are never overwritten.");result=SZ_ERROR_WRITE;goto done;}
 owned=1;out=fdopen(fd,"w+b");if(!out){close(fd);result=SZ_ERROR_WRITE;goto done;}
 for(i=0;i<count;i++){
  ZipCreateEntry *e=entries+i;CreateLzmaInput input;CreateLzmaOutput output;
  position=ftell(out);if(position<0){result=SZ_ERROR_WRITE;goto done;}e->offset=(UInt32)position;
  result=zipCreateHeader(out,e,e->directory?0:method,0);if(result!=SZ_OK)goto done;
  if(e->directory)continue;
  start=ftell(out);if(start<0){result=SZ_ERROR_WRITE;goto done;}
  source=fopen(tree->items[i].source,"rb");if(!source){result=SZ_ERROR_READ;goto done;}
  memset(&input,0,sizeof(input));input.iface.Read=createLzmaRead;input.file=source;input.remaining=e->size;input.base=processed;input.crc=CRC_INIT_VAL;input.requireEOF=1;
  output.iface.Write=createLzmaWrite;output.file=out;output.written=0;
  if(deflate)result=deflate==2?Z7Win16_Deflate64Encode(&input.iface,&output.iface,alloc):Z7Win16_DeflateEncode(&input.iface,&output.iface,alloc);
  else for(;;){size_t n=4096;result=createLzmaRead(&input.iface,buffer,&n);if(result!=SZ_OK||!n)break;if(createLzmaWrite(&output.iface,buffer,n)!=n){result=SZ_ERROR_WRITE;break;}}
  if(input.cancelled)result=SZ_ERROR_PROGRESS;
  if(result==SZ_OK&&input.remaining)result=SZ_ERROR_READ;
  if(result!=SZ_OK)goto done;e->crc=CRC_GET_DIGEST(input.crc);e->packed=(UInt32)output.written;processed+=e->size;
  if(fclose(source)){source=NULL;result=SZ_ERROR_READ;goto done;}source=NULL;
  end=ftell(out);if(end<start||(UInt64)(end-start)!=output.written||fseek(out,position,SEEK_SET)){result=SZ_ERROR_WRITE;goto done;}
  result=zipCreateHeader(out,e,e->directory?0:method,0);if(result!=SZ_OK)goto done;
  if(fseek(out,end,SEEK_SET)){result=SZ_ERROR_WRITE;goto done;}
 }
 central=ftell(out);if(central<0){result=SZ_ERROR_WRITE;goto done;}
 for(i=0;i<count;i++){result=zipCreateHeader(out,entries+i,entries[i].directory?0:method,1);if(result!=SZ_OK)goto done;}
 end=ftell(out);if(end<central){result=SZ_ERROR_WRITE;goto done;}
 {Byte bytes[22];CreateHeader h;h.data=bytes;h.used=0;h.failed=0;
  createLE(&h,0x06054b50UL,4);createLE(&h,0,2);createLE(&h,0,2);createLE(&h,count,2);createLE(&h,count,2);
  createLE(&h,(UInt32)(end-central),4);createLE(&h,(UInt32)central,4);createLE(&h,0,2);
  if(!createWriteBytes(out,bytes,22)||fflush(out)){result=SZ_ERROR_WRITE;goto done;}
 }
 if(fclose(out)){out=NULL;result=SZ_ERROR_WRITE;goto done;}out=NULL;
 /* A complete independent read through the archive engine precedes success. */
 {Input in;CLookToRead2 look;CSzArEx verify;
  in.file=fopen(path,"rb");if(!in.file){result=SZ_ERROR_READ;goto done;}in.iface.Read=readInput;in.iface.Seek=seekInput;
  LookToRead2_CreateVTable(&look,False);look.realStream=&in.iface;look.buf=buffer;look.bufSize=4096;LookToRead2_INIT(&look);SzArEx_Init(&verify);
  result=openArchive(&verify,&in,&look,alloc);
  if(result==SZ_OK&&verify.NumFiles!=count)result=SZ_ERROR_ARCHIVE;
  if(result==SZ_OK)result=testArchive(&verify,&in,&look,alloc);
  SzArEx_Free(&verify,alloc);if(fclose(in.file)&&result==SZ_OK)result=SZ_ERROR_READ;
 }
 if(result==SZ_OK)sprintf(zipCreateMessage,"ZIP archive created and verified: %u entries, %s.",count,deflate==2?"Deflate64":deflate?"Deflate":"Store");
 done:
 if(source)fclose(source);if(out)fclose(out);if(result!=SZ_OK&&owned&&remove(path))strcpy(zipCreateMessage,"ERROR: The incomplete ZIP archive could not be removed.");
 ISzAlloc_Free(alloc,entries);ISzAlloc_Free(alloc,buffer);return result;
}
static SRes createZip(const char *path,unsigned count,const char * const *sources,ISzAllocPtr alloc,int deflate){
 InputTree tree;SRes result;zipCreateMessage[0]=0;
 result=inputTreeBuild(&tree,count,sources,path,alloc);
 if(result==SZ_OK)result=createZipTree(path,&tree,alloc,deflate);
 else strcpy(zipCreateMessage,inputTreeMessage);
 inputTreeFree(&tree,alloc);return result;
}
#endif
