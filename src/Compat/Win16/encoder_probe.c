/* Probe for upstream LZMA encoding with segmented allocations.
   Writes the interoperable .lzma stream format, not a finished CLI product. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef __WATCOMC__
#include <io.h>
#else
#include <unistd.h>
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#include "../../C/LzmaEnc.h"
typedef struct {ISeqInStream iface;FILE *file;} EncoderInput;
typedef struct {ISeqOutStream iface;FILE *file;} EncoderOutput;
static unsigned allocationCall,activeAllocations,failAllocation;
static size_t shortRead;
static void *encoderAllocate(ISzAllocPtr self,size_t size){
 void *p;(void)self;allocationCall++;
 #ifndef __WATCOMC__
 if(size>65535UL)return NULL;
 #endif
 if(failAllocation&&allocationCall==failAllocation)return NULL;
 p=malloc(size);if(p)activeAllocations++;return p;
}
static void encoderFree(ISzAllocPtr self,void *pointer){(void)self;if(pointer){free(pointer);activeAllocations--;}}
static SRes encoderRead(ISeqInStreamPtr stream,void *data,size_t *size){
 EncoderInput *input=(EncoderInput *)stream;if(shortRead&&*size>shortRead)*size=shortRead;
 *size=fread(data,1,*size,input->file);return ferror(input->file)?SZ_ERROR_READ:SZ_OK;
}
static size_t encoderWrite(ISeqOutStreamPtr stream,const void *data,size_t size){
 return fwrite(data,1,size,((EncoderOutput *)stream)->file);
}
int main(int argc,char **argv){
 ISzAlloc allocator={encoderAllocate,encoderFree};CLzmaEncHandle encoder=NULL;CLzmaEncProps props;
 EncoderInput input;EncoderOutput output;Byte header[13];SizeT propertySize=5;UInt64 size;long length;unsigned i;
 int descriptor,owned=0;SRes result=SZ_ERROR_FAIL;
 if(argc<3||argc>6)return 7;input.file=output.file=NULL;
 if(argc>3)failAllocation=(unsigned)atoi(argv[3]);if(argc>4)shortRead=(size_t)atoi(argv[4]);
 input.file=fopen(argv[1],"rb");if(!input.file)return 2;
 if(fseek(input.file,0,SEEK_END)||(length=ftell(input.file))<0||fseek(input.file,0,SEEK_SET))goto done;
 size=(UInt64)length;
 descriptor=open(argv[2],O_WRONLY|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);if(descriptor<0)goto done;
 owned=1;output.file=fdopen(descriptor,"wb");if(!output.file){close(descriptor);goto done;}
 input.iface.Read=encoderRead;output.iface.Write=encoderWrite;
 encoder=LzmaEnc_Create(&allocator);if(!encoder){result=SZ_ERROR_MEM;goto done;}
 LzmaEncProps_Init(&props);props.dictSize=8192;props.numThreads=1;
 if(argc>5)props.algo=atoi(argv[5]);
 result=LzmaEnc_SetProps(encoder,&props);if(result!=SZ_OK)goto done;
 result=LzmaEnc_WriteProperties(encoder,header,&propertySize);if(result!=SZ_OK)goto done;
 for(i=0;i<8;i++){header[5+i]=(Byte)size;size>>=8;}
 if(fwrite(header,1,13,output.file)!=13){result=SZ_ERROR_WRITE;goto done;}
 result=LzmaEnc_Encode(encoder,&output.iface,&input.iface,NULL,&allocator,&allocator);
 done:
 if(encoder)LzmaEnc_Destroy(encoder,&allocator,&allocator);
 if(input.file)fclose(input.file);
 if(output.file&&fclose(output.file)&&result==SZ_OK)result=SZ_ERROR_WRITE;
 if(result!=SZ_OK&&owned)remove(argv[2]);
 printf("Encoder result=%d allocations=%u outstanding=%u\n",result,allocationCall,activeAllocations);
 return result==SZ_OK&&!activeAllocations?0:2;
}
