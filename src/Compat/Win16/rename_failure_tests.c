/* Fault injection for metadata-only archive updates; production I/O unchanged. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static long budget=-1;
static unsigned writes,attempts,failAt,outstanding;
static int failClose,failRename;
static FILE *lastOutput;
static size_t injectedWrite(const void *p,size_t unit,size_t count,FILE *file){
 size_t n=count,result;if(budget>=0&&(size_t)budget/unit<n)n=(size_t)budget/unit;
 result=fwrite(p,unit,n,file);writes++;lastOutput=file;if(budget>=0)budget-=(long)(result*unit);return result;
}
static int injectedClose(FILE *file){int fail=failClose&&file==lastOutput,result;if(file==lastOutput)lastOutput=NULL;result=fclose(file);return fail?EOF:result;}
static int injectedRename(const char *from,const char *to){if(failRename&&(strstr(from,".TMP")||(failRename==2&&strstr(from,".BAK")))){errno=EACCES;return -1;}return rename(from,to);}
#define fwrite injectedWrite
#define fclose injectedClose
#define rename injectedRename
#define main unusedConsoleMain
#include "console.c"
#undef main
#undef rename
#undef fclose
#undef fwrite
static void *faultAlloc(ISzAllocPtr p,size_t n){void *v;(void)p;if(++attempts==failAt)return NULL;v=malloc(n);if(v)outstanding++;return v;}
static void faultFree(ISzAllocPtr p,void *v){(void)p;if(v){outstanding--;free(v);}}
static int progressCancel(UInt64 pos){(void)pos;return !writes;}
static int copyFile(const char *from,const char *to){FILE *in=fopen(from,"rb"),*out;char b[4096];size_t n;int good=1;if(!in)return 0;out=fopen(to,"wb");if(!out){fclose(in);return 0;}while((n=fread(b,1,sizeof(b),in))!=0)if(fwrite(b,1,n,out)!=n){good=0;break;}if(ferror(in)||fclose(out))good=0;fclose(in);return good;}
static int sameFiles(const char *left,const char *right){FILE *a=fopen(left,"rb"),*b=fopen(right,"rb");int x,y,good=1;if(!a||!b)return 0;do{x=fgetc(a);y=fgetc(b);if(x!=y){good=0;break;}}while(x!=EOF);if(ferror(a)||ferror(b))good=0;fclose(a);fclose(b);return good;}
int main(int argc,char **argv){
 ISzAlloc alloc={faultAlloc,faultFree};SRes result;unsigned i,total=0;
 if(argc!=3)return 7;CrcGenerateTable();
 for(i=0;i<4;i++){
  if(!copyFile(argv[1],"FAULT.7Z"))return 2;
  budget=i==0?5000:-1;failClose=i==1;failRename=i==2;writes=0;lastOutput=NULL;testProgress=i==3?progressCancel:NULL;
  result=renameArchive("FAULT.7Z",argv[2],"RENAMED.TXT",&alloc);testProgress=NULL;
  if(result!=(i==3?SZ_ERROR_PROGRESS:SZ_ERROR_WRITE)||outstanding||!sameFiles(argv[1],"FAULT.7Z")){printf("FAIL fault %u result=%d blocks=%u\n",i,result,outstanding);return 2;}
  printf("PASS update fault %u: original archive unchanged\n",i);
 }
#ifdef __WATCOMC__
 budget=-1;failClose=0;failRename=2;
 if(!copyFile(argv[1],"FAULT.7Z"))return 2;
 result=renameArchive("FAULT.7Z",argv[2],"RENAMED.TXT",&alloc);
 if(result!=SZ_ERROR_WRITE||outstanding||!sameFiles(argv[1],"7R000000.BAK")||!strstr(renameMessage,"preserved")){puts("FAIL recovery backup");return 2;}
 if(rename("7R000000.BAK","FAULT.7Z"))return 2;
 puts("PASS commit and rollback failure retain the exact recovery backup");
#endif
 budget=-1;failClose=failRename=0;
 for(i=0;i<=total;i++){
  if(!copyFile(argv[1],"FAULT.7Z"))return 2;
  attempts=0;failAt=i;writes=0;lastOutput=NULL;
  result=renameArchive("FAULT.7Z",argv[2],"RENAMED.TXT",&alloc);
  if(!i){if(result!=SZ_OK)return 2;total=attempts;}
  else if(result!=SZ_ERROR_MEM||!sameFiles(argv[1],"FAULT.7Z")){printf("FAIL allocation %u result=%d\n",i,result);return 2;}
  if(outstanding){printf("FAIL leaked %u blocks\n",outstanding);return 2;}
 }
 remove("FAULT.7Z");printf("PASS all %u update allocation failures\n",total);return 0;
}
