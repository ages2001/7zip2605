/* Streaming adapter between the native file writer and upstream LZMA. */
#ifndef Z7_WIN16_CREATE_LZMA_H
#define Z7_WIN16_CREATE_LZMA_H
#include "../../C/LzmaEnc.h"
typedef struct {
 ISeqInStream iface;FILE *file;UInt64 remaining,base,read;UInt32 crc;int cancelled,requireEOF;
} CreateLzmaInput;
typedef struct {ISeqOutStream iface;FILE *file;UInt64 written;} CreateLzmaOutput;
static SRes createLzmaRead(ISeqInStreamPtr stream,void *buffer,size_t *size){
 CreateLzmaInput *input=(CreateLzmaInput *)stream;size_t wanted=*size;
 if(testProgress&&!testProgress(input->base+input->read)){input->cancelled=1;*size=0;return SZ_ERROR_PROGRESS;}
 if(input->remaining<wanted)wanted=(size_t)input->remaining;
 if(!wanted){*size=0;if(!input->requireEOF)return SZ_OK;return fgetc(input->file)==EOF&&!ferror(input->file)?SZ_OK:SZ_ERROR_READ;}
 *size=fread(buffer,1,wanted,input->file);if(*size!=wanted)return SZ_ERROR_READ;
 input->crc=CrcUpdate(input->crc,buffer,*size);input->remaining-=*size;input->read+=*size;return SZ_OK;
}
static size_t createLzmaWrite(ISeqOutStreamPtr stream,const void *buffer,size_t size){
 CreateLzmaOutput *output=(CreateLzmaOutput *)stream;size_t written=fwrite(buffer,1,size,output->file);
 output->written+=written;return written;
}
static SRes createLzmaRange(FILE *source,FILE *destination,UInt64 size,UInt64 base,
    UInt64 *packed,UInt32 *crc,Byte *properties,ISzAllocPtr allocator,int requireEOF){
 CLzmaEncHandle encoder;CLzmaEncProps props;CreateLzmaInput input;CreateLzmaOutput output;
 SRes result;SizeT propertySize=5;
 input.iface.Read=createLzmaRead;input.file=source;input.remaining=size;input.base=base;input.read=0;input.crc=CRC_INIT_VAL;input.cancelled=0;input.requireEOF=requireEOF;
 output.iface.Write=createLzmaWrite;output.file=destination;output.written=0;
 encoder=LzmaEnc_Create(allocator);if(!encoder)return SZ_ERROR_MEM;
 LzmaEncProps_Init(&props);props.dictSize=8192;props.numThreads=1;props.algo=0;
 result=LzmaEnc_SetProps(encoder,&props);
 if(result==SZ_OK)result=LzmaEnc_WriteProperties(encoder,properties,&propertySize);
 if(result==SZ_OK)result=LzmaEnc_Encode(encoder,&output.iface,&input.iface,NULL,allocator,allocator);
 /* The upstream encoder maps input callback errors to SZ_ERROR_READ. Keep
    cooperative cancellation distinct from a disk read failure in this UI. */
 if(input.cancelled)result=SZ_ERROR_PROGRESS;
 LzmaEnc_Destroy(encoder,allocator,allocator);
 if(result==SZ_OK&&input.remaining)result=SZ_ERROR_READ;
 *packed=output.written;*crc=CRC_GET_DIGEST(input.crc);return result;
}
static SRes createLzmaStream(FILE *source,FILE *destination,UInt64 size,UInt64 base,
    UInt64 *packed,UInt32 *crc,Byte *properties,ISzAllocPtr allocator){
 return createLzmaRange(source,destination,size,base,packed,crc,properties,allocator,1);
}
#endif
