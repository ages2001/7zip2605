/* Independent raw-stream probe and allocation/read/write fault injection. */
#include <stdio.h>
#include <stdlib.h>
#include "deflate_encode.h"
typedef struct {ISeqInStream iface;FILE *file;size_t shortRead;} Input;
typedef struct {ISeqOutStream iface;FILE *file;} Output;
static unsigned attempts,failAt,outstanding;
static int failRead,failWrite;
static void *probeAlloc(ISzAllocPtr a,size_t n){void *p;(void)a;if(++attempts==failAt||n>65520UL)return NULL;p=malloc(n);if(p)outstanding++;return p;}
static void probeFree(ISzAllocPtr a,void *p){(void)a;if(p){outstanding--;free(p);}}
static SRes probeRead(ISeqInStreamPtr stream,void *data,size_t *size){Input *s=(Input *)stream;if(failRead)return SZ_ERROR_READ;if(s->shortRead&&*size>s->shortRead)*size=s->shortRead;*size=fread(data,1,*size,s->file);return ferror(s->file)?SZ_ERROR_READ:SZ_OK;}
static size_t probeWrite(ISeqOutStreamPtr stream,const void *data,size_t size){Output *s=(Output *)stream;if(failWrite)return 0;return fwrite(data,1,size,s->file);}
int main(int argc,char **argv){
 Input in;Output out;ISzAlloc alloc={probeAlloc,probeFree};SRes result;
 if(argc<3||argc>7)return 7;
 if(argc>3)failAt=(unsigned)atoi(argv[3]);if(argc>4){failRead=atoi(argv[4])==1;failWrite=atoi(argv[4])==2;}
 in.shortRead=argc>5?(size_t)atoi(argv[5]):0;in.iface.Read=probeRead;out.iface.Write=probeWrite;
 in.file=fopen(argv[1],"rb");if(!in.file)return 2;out.file=fopen(argv[2],"wb");if(!out.file){fclose(in.file);return 2;}
 result=argc==7&&atoi(argv[6])?Z7Win16_Deflate64Encode(&in.iface,&out.iface,&alloc):Z7Win16_DeflateEncode(&in.iface,&out.iface,&alloc);fclose(in.file);if(fclose(out.file)&&result==SZ_OK)result=SZ_ERROR_WRITE;
 if(result!=SZ_OK)remove(argv[2]);printf("Deflate result=%d allocations=%u outstanding=%u\n",result,attempts,outstanding);return result==SZ_OK&&!outstanding?0:2;
}
