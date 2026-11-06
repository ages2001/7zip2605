/* Host-side fault injection for extraction rollback. Run in an empty fixture
   directory. Production file I/O is unchanged outside this translation unit. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long writeBudget=-1;
static unsigned outputWrites;
static int failClose;
static FILE *lastOutput;
static size_t injectedWrite(const void *data,size_t size,size_t count,FILE *file){
 size_t actual=count,result;
 if(writeBudget>=0&&(size_t)writeBudget/size<actual)actual=(size_t)writeBudget/size;
 result=fwrite(data,size,actual,file);outputWrites++;lastOutput=file;
 if(writeBudget>=0)writeBudget-=(long)(result*size);
 return result;
}
static int injectedClose(FILE *file){
 int isOutput=file==lastOutput,fail=failClose&&isOutput,result;
 if(isOutput)lastOutput=NULL;result=fclose(file);
 return fail?EOF:result;
}
#define fwrite injectedWrite
#define fclose injectedClose
#define main unusedConsoleMain
#include "console.c"
#undef main
#undef fclose
#undef fwrite
static int cancelAfterWriting(UInt64 position){(void)position;return outputWrites==0;}
static int flatExtraction;
static int runCase(const char *archivePath,const char *output,int kind){
 Input input;CLookToRead2 look;CSzArEx archive;ISzAlloc allocator={allocate,release};
 Byte buffer[4096];SRes result;struct stat info;
 input.iface.Read=readInput;input.iface.Seek=seekInput;input.file=fopen(archivePath,"rb");if(!input.file)return 0;
 LookToRead2_CreateVTable(&look,False);look.realStream=&input.iface;look.buf=buffer;look.bufSize=sizeof(buffer);LookToRead2_INIT(&look);SzArEx_Init(&archive);
 result=openArchive(&archive,&input,&look,&allocator);
 if(result!=SZ_OK){fclose(input.file);return 0;}
 writeBudget=kind==1?5000:-1;failClose=kind==2;outputWrites=0;lastOutput=NULL;
 testProgress=kind==3?cancelAfterWriting:NULL;
 result=extractArchiveMode(&archive,&input,&look,&allocator,output,flatExtraction);testProgress=NULL;
 SzArEx_Free(&archive,&allocator);fclose(input.file);
 if(result!=(kind==3?SZ_ERROR_PROGRESS:SZ_ERROR_WRITE)||!outputWrites||!stat(output,&info)){
  printf("FAIL fault %d result %d writes %u\n",kind,result,outputWrites);return 0;
 }
 printf("PASS fault %d, result %d; partial output removed\n",kind,result);return 1;
}
static int runCreateCase(const char *source,const char *output,int kind,int lzma){
 ISzAlloc allocator={allocate,release};SRes result;struct stat info;
 writeBudget=kind==1?64:-1;failClose=kind==2;outputWrites=0;lastOutput=NULL;
 testProgress=kind==3?cancelAfterWriting:NULL;
 result=createArchiveMethod(output,1,&source,&allocator,lzma);testProgress=NULL;
 if(result!=(kind==3?SZ_ERROR_PROGRESS:SZ_ERROR_WRITE)||!outputWrites||!stat(output,&info)){
  printf("FAIL creation fault %d result %d writes %u\n",kind,result,outputWrites);return 0;
 }
 printf("PASS %s creation fault %d, result %d; incomplete archive removed\n",lzma?"LZMA":"Copy",kind,result);return 1;
}
int main(int argc,char **argv){int ok;if(argc!=2&&argc!=3)return 7;flatExtraction=argc==3&&!strcmp(argv[2],"e");CrcGenerateTable();
 if(!validDosDestination("C:\\OUT")||!validDosDestination("..\\OUT")
     ||!validDosDestination("C:\\PARENT\\OUT.001")
     ||validDosDestination("C:\\D5TESTOUT")||validDosDestination("C:\\NUL")
     ||validDosDestination("C:\\OUT.1234")||validDosDestination("C:\\OUT.")
     ||validDosDestination("C:\\PARENT\\..")||validDosDestination("\\\\SERVER\\OUT"))return 2;
 puts("PASS DOS destination validation prevents silent 8.3 truncation");
 ok=runCase(argv[1],"SHORTOUT",1);ok&=runCase(argv[1],"CLOSEOUT",2);ok&=runCase(argv[1],"CANCOUT",3);
 ok&=runCreateCase(argv[1],"SHORT.7Z",1,0);ok&=runCreateCase(argv[1],"CLOSE.7Z",2,0);ok&=runCreateCase(argv[1],"CANCEL.7Z",3,0);
 ok&=runCreateCase(argv[1],"LZSHORT.7Z",1,1);ok&=runCreateCase(argv[1],"LZCLOSE.7Z",2,1);ok&=runCreateCase(argv[1],"LZCANCEL.7Z",3,1);return ok?0:2;
}
