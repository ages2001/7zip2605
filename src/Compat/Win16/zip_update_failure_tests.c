/* Native and sanitizer fault injection for ZIP transactions. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static long budget=-1;static unsigned writes,attempts,failAt,outstanding;
static int closeFailure,renameFailure,corrupt,stageClosed,candidateShort;
static FILE *stageOutput,*candidateOutput;
static size_t faultWrite(const void *data,size_t unit,size_t count,FILE *file){
 size_t n=count,result;
 if(!stageClosed&&!stageOutput)stageOutput=file;
 if(stageClosed&&!candidateOutput)candidateOutput=file;
 if(budget>=0&&(size_t)budget/unit<n)n=(size_t)budget/unit;
 if(candidateShort&&stageClosed&&n)n--;
 result=fwrite(data,unit,n,file);writes++;if(budget>=0)budget-=(long)(result*unit);return result;
}
static int faultClose(FILE *file){int isStage=file==stageOutput,isCandidate=file==candidateOutput,result,fail;
 fail=(closeFailure==1&&isStage)||(closeFailure==2&&isCandidate);
 if((corrupt==1&&isStage)||(corrupt==2&&isCandidate)){
  int ch;fflush(file);fseek(file,49,SEEK_SET);ch=fgetc(file);fseek(file,49,SEEK_SET);fputc(ch^1,file);corrupt=0;
 }
 if(isStage){stageOutput=NULL;stageClosed=1;}if(isCandidate)candidateOutput=NULL;
 result=fclose(file);return fail?EOF:result;
}
static int faultRename(const char *from,const char *to){if(renameFailure&&(strstr(from,".TMP")||(renameFailure==2&&strstr(from,".BAK")))){errno=EACCES;return -1;}return rename(from,to);}
#define fwrite faultWrite
#define fclose faultClose
#define rename faultRename
#define main unusedConsoleMain
#include "console.c"
#undef main
#undef rename
#undef fclose
#undef fwrite
static void *faultAlloc(ISzAllocPtr p,size_t n){void *v;(void)p;if(++attempts==failAt)return NULL;v=malloc(n);if(v)outstanding++;return v;}
static void faultFree(ISzAllocPtr p,void *v){(void)p;if(v){outstanding--;free(v);}}
static int cancelProgress(UInt64 n){(void)n;return !writes;}
static int copyFile(const char *from,const char *to){FILE *in=fopen(from,"rb"),*out;char b[4096];size_t n;int good=1;if(!in)return 0;out=fopen(to,"wb");if(!out){fclose(in);return 0;}while((n=fread(b,1,sizeof(b),in))!=0)if(fwrite(b,1,n,out)!=n){good=0;break;}if(ferror(in)||fclose(out))good=0;fclose(in);return good;}
static int sameFiles(const char *left,const char *right){FILE *a=fopen(left,"rb"),*b=fopen(right,"rb");int x,y,good=1;if(!a||!b)return 0;do{x=fgetc(a);y=fgetc(b);if(x!=y){good=0;break;}}while(x!=EOF);if(ferror(a)||ferror(b))good=0;fclose(a);fclose(b);return good;}
static int leftovers(void){const char *exts[3]={"TMP","ZIP","BAK"};unsigned i,j;char path[20];struct stat st;for(i=0;i<3;i++)for(j=0;j<3;j++){sprintf(path,"7R%06X.%s",j,exts[i]);if(!stat(path,&st))return 1;}return 0;}
static void reset(int deleting){budget=-1;writes=attempts=failAt=0;closeFailure=renameFailure=corrupt=candidateShort=0;stageClosed=deleting;stageOutput=candidateOutput=NULL;testProgress=NULL;}
static SRes operate(const char *source,ISzAllocPtr alloc,int method){
 const char *names[2];names[0]=source;names[1]="RENAMED.TXT";
 return updateZip("ZFAULT.ZIP",method==3?2:1,names,alloc,method==4?2:method!=0,method==3?2:method==2);
}
int main(int argc,char **argv){
 ISzAlloc alloc={faultAlloc,faultFree};SRes result;unsigned i,total=0;int method,deleting;
 if(argc!=4)return 7;method=atoi(argv[3]);deleting=method==2||method==3;CrcGenerateTable();
 for(i=0;i<8;i++){
  if(deleting&&(i==1||i==5))continue;
  if(!copyFile(argv[1],"ZFAULT.ZIP"))return 2;reset(deleting);
  if(i==0)budget=80;if(i==1)closeFailure=1;if(i==2)closeFailure=2;if(i==3)renameFailure=1;
  if(i==4)testProgress=cancelProgress;if(i==5)corrupt=1;if(i==6)corrupt=2;if(i==7)candidateShort=1;
  result=operate(argv[2],&alloc,method);testProgress=NULL;
  if(result==SZ_OK||outstanding||corrupt||leftovers()||!sameFiles(argv[1],"ZFAULT.ZIP")||(i==4&&result!=SZ_ERROR_PROGRESS)){
   printf("FAIL ZIP fault %u result=%d blocks=%u corrupt=%d\n",i,result,outstanding,corrupt);return 2;
  }
  printf("PASS ZIP transaction fault %u: original bytes and cleanup verified\n",i);
 }
#ifdef __WATCOMC__
 if(!copyFile(argv[1],"ZFAULT.ZIP"))return 2;reset(deleting);renameFailure=2;
 result=operate(argv[2],&alloc,method);
 if(result!=SZ_ERROR_WRITE||outstanding||!sameFiles(argv[1],"7R000000.BAK")||!strstr(updateMessage,"preserved")){puts("FAIL ZIP recovery backup");return 2;}
 if(rename("7R000000.BAK","ZFAULT.ZIP"))return 2;
 puts("PASS ZIP commit and restore failure retain exact recovery backup");
#endif
 for(i=0;i<=total;i++){
  if(!copyFile(argv[1],"ZFAULT.ZIP"))return 2;reset(deleting);failAt=i;
  result=operate(argv[2],&alloc,method);
  if(!i){if(result!=SZ_OK){printf("FAIL baseline %d\n",result);return 2;}total=attempts;}
  else if(result!=SZ_ERROR_MEM||!sameFiles(argv[1],"ZFAULT.ZIP")){printf("FAIL ZIP allocation %u result=%d\n",i,result);return 2;}
  if(outstanding||leftovers()){printf("FAIL ZIP cleanup blocks=%u\n",outstanding);return 2;}
 }
 remove("ZFAULT.ZIP");printf("PASS all %u ZIP transaction allocation failures\n",total);return 0;
}
