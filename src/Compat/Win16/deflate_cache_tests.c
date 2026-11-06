/* End-to-end Deflate cache failures, also built for native DOS.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "deflate_stream.h"
#include "../../C/7zCrc.h"
static unsigned fault,allocations,failAt,live;
static char retained[260];
static size_t cacheRead(void *p,size_t s,size_t n,FILE *f){return fault==1?0:fread(p,s,n,f);}
static size_t cacheWrite(const void *p,size_t s,size_t n,FILE *f){return fwrite(p,s,fault==2&&n?n-1:n,f);}
static int cacheSeek(FILE *f,long n,int w){return fault==3?-1:fseek(f,n,w);}
static FILE *cacheFdopen(int fd,const char *m){return fault==4?NULL:fdopen(fd,m);}
static int cacheClose(FILE *f){int r=fclose(f);return fault==5?EOF:r;}
static int cacheRemove(const char *p){if(fault==6){strcpy(retained,p);return -1;}return remove(p);}
#define fread cacheRead
#define fwrite cacheWrite
#define fseek cacheSeek
#define fdopen cacheFdopen
#define fclose cacheClose
#define remove cacheRemove
#include "paged_dictionary.c"
#undef remove
#undef fclose
#undef fdopen
#undef fseek
#undef fwrite
#undef fread
static void *allocate(ISzAllocPtr a,size_t n){void *p;(void)a;if(++allocations==failAt)return NULL;p=malloc(n);if(p)live++;return p;}
static void release(ISzAllocPtr a,void *p){(void)a;if(p){live--;free(p);}}
typedef struct {ISeekInStream v;FILE *file;} Input;
typedef struct {ISeqOutStream v;FILE *file;UInt32 count;int fail;} Output;
static SRes inputRead(ISeekInStreamPtr p,void *data,size_t *size){Input *s=(Input *)p;if(*size>13)*size=13;*size=fread(data,1,*size,s->file);return ferror(s->file)?SZ_ERROR_READ:SZ_OK;}
static SRes inputSeek(ISeekInStreamPtr p,Int64 *n,ESzSeek w){(void)p;(void)n;(void)w;return SZ_ERROR_UNSUPPORTED;}
static size_t outputWrite(ISeqOutStreamPtr p,const void *data,size_t size){Output *s=(Output *)p;size_t i;const Byte *b=(const Byte *)data;if(s->fail)return 0;for(i=0;i<size;i++)if(fgetc(s->file)!=b[i])return 0;s->count+=size;return size;}
#define CHECK(c) do{checks++;if(!(c)){printf("FAIL line %u fault %u allocation %u\n",(unsigned)__LINE__,fault,failAt);return 2;}}while(0)
int main(int argc,char **argv){
 Input in;Output out;ISzAlloc a={allocate,release};UInt32 crc;SRes r;unsigned checks=0,mode,n,needed;long packed,unpacked;struct stat st;
 int extended=argc==4&&!strcmp(argv[3],"64");
 if(argc!=3&&!extended)return 7;CrcGenerateTable();in.v.Read=inputRead;in.v.Seek=inputSeek;out.v.Write=outputWrite;
 for(mode=0;mode<2;mode++){
  needed=0;
  for(n=0;n<=needed;n++){
   in.file=fopen(argv[1],"rb");out.file=fopen(argv[2],"rb");CHECK(in.file&&out.file);
   CHECK(!fseek(in.file,0,SEEK_END));packed=ftell(in.file);rewind(in.file);CHECK(!fseek(out.file,0,SEEK_END));unpacked=ftell(out.file);rewind(out.file);
   out.count=0;out.fail=0;allocations=0;failAt=n;crc=0x12345678UL;
   r=extended?Z7Win16_Inflate64WithCache(&in.v,&out.v,packed,unpacked,&a,&crc,mode):Z7Win16_InflateWithCache(&in.v,&out.v,packed,unpacked,&a,&crc,mode);
   CHECK(r==(n?SZ_ERROR_MEM:SZ_OK));CHECK(!live);CHECK(n?(out.count==0&&crc==0x12345678UL):(out.count==(UInt32)unpacked&&fgetc(out.file)==EOF));
   if(!n)needed=allocations;CHECK(!fclose(in.file));CHECK(!fclose(out.file));
  }
  printf("PASS %s history and all %u allocation failures\n",mode?"paged":"RAM",needed);
 }
 failAt=0;
 for(fault=1;fault<=7;fault++){
  in.file=fopen(argv[1],"rb");out.file=fopen(argv[2],"rb");CHECK(in.file&&out.file);out.count=0;out.fail=fault==7;crc=0x12345678UL;
  r=extended?Z7Win16_Inflate64WithCache(&in.v,&out.v,packed,unpacked,&a,&crc,1):Z7Win16_InflateWithCache(&in.v,&out.v,packed,unpacked,&a,&crc,1);
  CHECK(r==(fault==1||fault==3?SZ_ERROR_READ:SZ_ERROR_WRITE));CHECK(!live&&crc==0x12345678UL);
  CHECK(!fclose(in.file));CHECK(!fclose(out.file));
  if(fault==6){CHECK(retained[0]&&!stat(retained,&st));CHECK(!remove(retained));retained[0]=0;}
 }
 printf("PASS %u checks; cache read/write/seek/open/close/remove and output failures reach caller; no successful CRC or allocation leak\n",checks);return 0;
}
