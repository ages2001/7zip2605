/* Real files larger than a 16-bit segment, through shared upstream decoders. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stream_folder.h"
#include "../../C/7zCrc.h"
#ifdef __WINDOWS__
#include <windows.h>
#endif
static const char pattern[]="Streaming 7-Zip across 16-bit segment boundaries.\r\n";
typedef struct {ISeekInStream iface;FILE *file;unsigned shortReads;unsigned remaining;int truncate;} Input;
typedef struct {ISeqOutStream iface;UInt32 size;int valid;int failWrite;UInt32 crc;} Output;
static SRes readInput(ISeekInStreamPtr p,void *buffer,size_t *n){Input *s=(Input *)p;if(s->shortReads && *n>13)*n=13;if(s->truncate && *n>s->remaining)*n=s->remaining;*n=fread(buffer,1,*n,s->file);if(s->truncate)s->remaining-=*n;return ferror(s->file)?SZ_ERROR_READ:SZ_OK;}
static SRes seekInput(ISeekInStreamPtr p,Int64 *pos,ESzSeek origin){Input *s=(Input *)p;long n=(long)*pos;if((Int64)n!=*pos)return SZ_ERROR_UNSUPPORTED;if(fseek(s->file,n,(int)origin))return SZ_ERROR_READ;n=ftell(s->file);if(n<0)return SZ_ERROR_READ;*pos=n;return SZ_OK;}
static size_t writeOutput(ISeqOutStreamPtr p,const void *buffer,size_t n){Output *s=(Output *)p;size_t i;const Byte *b=(const Byte *)buffer;if(s->failWrite)return 0;for(i=0;i<n;i++)if(b[i]!=(Byte)pattern[(s->size+i)%(sizeof(pattern)-1)])s->valid=0;s->size+=n;s->crc=CrcUpdate(s->crc,buffer,n);return n;}
static void *allocate(ISzAllocPtr p,size_t n){(void)p;return malloc(n);}
static void release(ISzAllocPtr p,void *v){(void)p;free(v);}
static int test(FILE *log,const char *path,unsigned shortReads,int failWrite){
 Input input;Output output;CLookToRead2 stream;CSzArEx archive;ISzAlloc allocator={allocate,release};Byte *look;SRes result;int valid;
 input.iface.Read=readInput;input.iface.Seek=seekInput;input.shortReads=shortReads;input.truncate=0;input.remaining=10;input.file=fopen(path,"rb");if(!input.file){fprintf(log,"FAIL open %s\n",path);return 0;}
 look=(Byte *)malloc(4096);if(!look){fclose(input.file);return 0;}
 LookToRead2_CreateVTable(&stream,False);stream.realStream=&input.iface;stream.buf=look;stream.bufSize=4096;LookToRead2_INIT(&stream);SzArEx_Init(&archive);
 output.iface.Write=writeOutput;output.size=0;output.valid=1;output.failWrite=failWrite==1;output.crc=CRC_INIT_VAL;
 result=SzArEx_Open(&archive,&stream.vt,&allocator,&allocator);
 if(result==SZ_OK){if(archive.db.NumFolders!=1||archive.NumFiles!=1)result=SZ_ERROR_ARCHIVE;else {
   Byte crcDefined=0x80;UInt32 badCrc=0;CSzBitUi32s saved=archive.db.FolderCRCs;
   if(failWrite==2){archive.db.FolderCRCs.Defs=&crcDefined;archive.db.FolderCRCs.Vals=&badCrc;}
   if(failWrite==3)input.truncate=1;
   result=Z7Win16_DecodeFolder(&archive,0,&input.iface,&output.iface,&allocator);
   archive.db.FolderCRCs=saved;
  }}
 valid=failWrite?(result==(failWrite==1?SZ_ERROR_WRITE:failWrite==2?SZ_ERROR_CRC:SZ_ERROR_INPUT_EOF)):(result==SZ_OK && output.valid && output.size==262267UL);
 if(valid&&!failWrite&&SzBitWithVals_Check(&archive.CRCs,0))valid=CRC_GET_DIGEST(output.crc)==archive.CRCs.Vals[0];
 fprintf(log,"%s %s shortReads=%u fault=%d result=%d bytes=%lu\n",valid?"PASS":"FAIL",path,shortReads,failWrite,result,output.size);fflush(log);
 SzArEx_Free(&archive,&allocator);free(look);fclose(input.file);return valid;
}
static int run(void){FILE *log=fopen("C:\\STREAM16.TXT","w");int valid=1;if(!log)return 2;CrcGenerateTable();
#ifdef __WINDOWS__
 fprintf(log,"GetWinFlags=%lu\n",GetWinFlags());
#endif
 valid&=test(log,"C:\\S16COPY.7Z",0,0);valid&=test(log,"C:\\S16LZMA.7Z",0,0);valid&=test(log,"C:\\S16LZM2.7Z",0,0);valid&=test(log,"C:\\S16LZM2.7Z",1,0);valid&=test(log,"C:\\S16LZM2.7Z",0,1);valid&=test(log,"C:\\S16LZM2.7Z",0,2);valid&=test(log,"C:\\S16LZM2.7Z",0,3);
 fprintf(log,"%s ALL STREAM TESTS\n",valid?"PASS":"FAIL");fclose(log);return valid?0:1;}
#ifdef __WINDOWS__
int PASCAL WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show){int r;(void)instance;(void)previous;(void)command;(void)show;r=run();MessageBox(0,r?"FAIL: streaming archive tests.":"PASS: Copy, LZMA and LZMA2 streams exceed 256 KiB.","Native Win16 archive tests",MB_OK);return r;}
#else
int main(void){return run();}
#endif
