/* Fault injection for exclusive ZIP creation and complete candidate validation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long budget=-1;static unsigned writes,attempts,failAt,outstanding;
static int failClose,corrupt;static FILE *lastOutput;
static size_t zipFaultWrite(const void *p,size_t unit,size_t count,FILE *file){size_t n=count,result;if(budget>=0&&(size_t)budget/unit<n)n=(size_t)budget/unit;result=fwrite(p,unit,n,file);writes++;lastOutput=file;if(budget>=0)budget-=(long)(result*unit);return result;}
static int zipFaultClose(FILE *file){int fail=failClose&&file==lastOutput,result;if(file==lastOutput){if(corrupt){int byte;fflush(file);fseek(file,49,SEEK_SET);byte=fgetc(file);fseek(file,49,SEEK_SET);fputc(byte^1,file);corrupt=0;}lastOutput=NULL;}result=fclose(file);return fail?EOF:result;}
#define fwrite zipFaultWrite
#define fclose zipFaultClose
#define main unusedConsoleMain
#include "console.c"
#undef main
#undef fclose
#undef fwrite
static void *faultAlloc(ISzAllocPtr p,size_t n){void *v;(void)p;if(++attempts==failAt)return NULL;v=malloc(n);if(v)outstanding++;return v;}
static void faultFree(ISzAllocPtr p,void *v){(void)p;if(v){outstanding--;free(v);}}
static int cancelProgress(UInt64 position){(void)position;return !writes;}
static int missing(void){struct stat st;return stat("ZFAULT.ZIP",&st)!=0;}
int main(int argc,char **argv){
 ISzAlloc alloc={faultAlloc,faultFree};SRes result;unsigned i,total=0;int method;
 if(argc!=3)return 7;method=atoi(argv[2]);CrcGenerateTable();
 if(!missing())return 7;
 for(i=0;i<4;i++){
  budget=i==0?80:-1;failClose=i==1;testProgress=i==2?cancelProgress:NULL;corrupt=i==3;writes=0;lastOutput=NULL;
  result=createZip("ZFAULT.ZIP",1,(const char * const *)(argv+1),&alloc,method);testProgress=NULL;
  if(result==SZ_OK||outstanding||!missing()||corrupt||(i==2&&result!=SZ_ERROR_PROGRESS)){printf("FAIL ZIP fault %u result=%d blocks=%u\n",i,result,outstanding);return 2;}
  printf("PASS ZIP fault %u: partial output removed\n",i);
 }
 budget=-1;failClose=0;
 for(i=0;i<=total;i++){
  attempts=0;failAt=i;writes=0;lastOutput=NULL;
  result=createZip("ZFAULT.ZIP",1,(const char * const *)(argv+1),&alloc,method);
  if(!i){if(result!=SZ_OK)return 2;total=attempts;if(remove("ZFAULT.ZIP"))return 2;}
  else if(result!=SZ_ERROR_MEM||!missing()){printf("FAIL ZIP allocation %u result=%d\n",i,result);return 2;}
  if(outstanding){printf("FAIL ZIP leak %u\n",outstanding);return 2;}
 }
 printf("PASS all %u ZIP allocation failures\n",total);return 0;
}
