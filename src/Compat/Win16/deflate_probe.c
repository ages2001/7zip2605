/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdlib.h>
#include "deflate_stream.h"
#include "../../C/7zCrc.h"
typedef struct {ISeekInStream v;FILE *f;unsigned chunk;} Input;
typedef struct {ISeqOutStream v;FILE *f;int fail;} Output;
static unsigned allocations,failAt,outstanding,maxAllocation;
static void *alloc(ISzAllocPtr a,size_t n){void *p;(void)a;if(++allocations==failAt||(maxAllocation&&n>maxAllocation))return NULL;p=malloc(n);if(p)outstanding++;return p;}
static void release(ISzAllocPtr a,void *p){(void)a;if(p){outstanding--;free(p);}}
static SRes readIn(ISeekInStreamPtr p,void *data,size_t *size){Input *s=(Input *)p;if(*size>s->chunk)*size=s->chunk;*size=fread(data,1,*size,s->f);return ferror(s->f)?SZ_ERROR_READ:SZ_OK;}
static SRes seekIn(ISeekInStreamPtr p,Int64 *pos,ESzSeek origin){(void)p;(void)pos;(void)origin;return SZ_ERROR_UNSUPPORTED;}
static size_t writeOut(ISeqOutStreamPtr p,const void *data,size_t size){Output *s=(Output *)p;if(s->fail)return 0;return fwrite(data,1,size,s->f);}
int main(int argc,char **argv){Input in;Output out;ISzAlloc a={alloc,release};SRes result;long packed;UInt32 crc=0;
 if(argc<4)return 7;
 in.v.Read=readIn;in.v.Seek=seekIn;in.chunk=argc>4?(unsigned)atoi(argv[4]):4096;
 failAt=argc>5?(unsigned)atoi(argv[5]):0;
 maxAllocation=argc>7?(unsigned)atoi(argv[7]):0;
 out.v.Write=writeOut;out.fail=argc>6?atoi(argv[6]):0;
 in.f=fopen(argv[1],"rb");if(!in.f)return 2;
 fseek(in.f,0,SEEK_END);packed=ftell(in.f);rewind(in.f);
 out.f=fopen(argv[2],"wb");if(!out.f){fclose(in.f);return 2;}
 CrcGenerateTable();result=(argc>9&&atoi(argv[9])?Z7Win16_Inflate64WithCache:Z7Win16_InflateWithCache)(&in.v,&out.v,(UInt64)packed,(UInt64)strtoul(argv[3],NULL,10),&a,&crc,argc>8?atoi(argv[8]):0);
 if(fclose(out.f)&&result==SZ_OK)result=SZ_ERROR_WRITE;fclose(in.f);
 if(result!=SZ_OK)remove(argv[2]);
 printf("result=%d crc=%08lx allocations=%u outstanding=%u\n",result,(unsigned long)crc,allocations,outstanding);
 return result==SZ_OK && !outstanding?0:2;
}
