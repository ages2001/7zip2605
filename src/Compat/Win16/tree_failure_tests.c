/* Exercise recursive planning and writer rollback on native DOS as well as ASan. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned writes,attempts,failAt,outstanding,progressCalls;
static long budget=-1;
static int closeFailure,cancelMode;
static FILE *lastOutput;
static size_t treeFaultWrite(const void *data,size_t unit,size_t count,FILE *file){
 size_t n=count,result;if(budget>=0&&n>(size_t)budget/unit)n=(size_t)budget/unit;
 result=fwrite(data,unit,n,file);lastOutput=file;writes++;if(budget>=0)budget-=(long)(result*unit);return result;
}
static int treeFaultClose(FILE *file){int fail=closeFailure&&file==lastOutput,result;if(file==lastOutput)lastOutput=NULL;result=fclose(file);return fail?EOF:result;}
#define fwrite treeFaultWrite
#define fclose treeFaultClose
#define main unusedConsoleMain
#include "console.c"
#undef main
#undef fclose
#undef fwrite
static void *treeAlloc(ISzAllocPtr p,size_t n){void *v;(void)p;if(++attempts==failAt)return NULL;v=malloc(n);if(v)outstanding++;return v;}
static void treeFree(ISzAllocPtr p,void *v){(void)p;if(v){outstanding--;free(v);}}
static int treeCancel(UInt64 position){(void)position;progressCalls++;return cancelMode==1?progressCalls<3:!writes;}
static int treeCopy(const char *from,const char *to){FILE *in=fopen(from,"rb"),*out;char b[256];size_t n;int ok=1;if(!in)return 0;out=fopen(to,"wb");if(!out){fclose(in);return 0;}while((n=fread(b,1,sizeof(b),in))!=0)if(fwrite(b,1,n,out)!=n){ok=0;break;}if(ferror(in)||fclose(out))ok=0;fclose(in);return ok;}
static int treeSame(const char *left,const char *right){FILE *a=fopen(left,"rb"),*b=fopen(right,"rb");int x,y,ok=1;if(!a||!b){if(a)fclose(a);if(b)fclose(b);return 0;}do{x=fgetc(a);y=fgetc(b);if(x!=y){ok=0;break;}}while(x!=EOF);if(ferror(a)||ferror(b))ok=0;fclose(a);fclose(b);return ok;}
static SRes treeOperation(int operation,int method,const char *path,const char *source,ISzAllocPtr alloc){
 if(operation==0)return createArchiveMethod(path,1,&source,alloc,method);
 if(operation==1)return createZip(path,1,&source,alloc,method);
 return updateAnyArchive(path,1,&source,alloc,method,0);
}
int main(int argc,char **argv){
 ISzAlloc alloc={treeAlloc,treeFree};int op,method;unsigned i,total;SRes result;struct stat st;
 if(argc!=4)return 7;CrcGenerateTable();
 for(op=0;op<4;op++)for(method=0;method<2;method++){
  const char *path=op&1?"TFAULT.ZIP":"TFAULT.7Z",*original=argv[(op&1)+1];
  if(!stat(path,&st))return 7;
  for(i=0;i<4;i++){
   if(op>=2&&!treeCopy(original,path))return 2;
   writes=attempts=progressCalls=failAt=0;lastOutput=NULL;budget=i==0?80:-1;closeFailure=i==1;cancelMode=i>=2?(int)i-1:0;testProgress=cancelMode?treeCancel:NULL;
   result=treeOperation(op,method,path,argv[3],&alloc);testProgress=NULL;
   if(result==SZ_OK||outstanding||(cancelMode&&result!=SZ_ERROR_PROGRESS)||(op>=2?!treeSame(original,path):!stat(path,&st))){printf("FAIL tree operation=%d method=%d fault=%u result=%d blocks=%u\n",op,method,i,result,outstanding);return 2;}
   if(op>=2&&remove(path))return 2;
  }
  budget=-1;closeFailure=cancelMode=0;total=0;
  for(i=0;i<=total;i++){
   if(op>=2&&!treeCopy(original,path))return 2;
   writes=attempts=progressCalls=0;failAt=i;lastOutput=NULL;result=treeOperation(op,method,path,argv[3],&alloc);
   if(!i){if(result!=SZ_OK){printf("FAIL tree baseline op=%d result=%d\n",op,result);return 2;}total=attempts;}
   else if(result!=SZ_ERROR_MEM||(op>=2?!treeSame(original,path):!stat(path,&st))){printf("FAIL tree allocation op=%d method=%d index=%u result=%d\n",op,method,i,result);return 2;}
   if(outstanding){printf("FAIL tree leaked %u blocks\n",outstanding);return 2;}
   if((!i||op>=2)&&remove(path))return 2;
  }
  printf("PASS tree operation=%d method=%d: cancellation, partial writes, close errors and all %u allocation failures\n",op,method,total);
 }
 return 0;
}
