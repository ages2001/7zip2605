/* Selected-extraction integration/fault probe; never a distribution binary. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long writeBudget=-1;
static unsigned writes;
static int failClose;
static FILE *lastOutput;
static size_t checkedWrite(const void *p,size_t size,size_t count,FILE *f){
 size_t n=count,result;if(writeBudget>=0&&(size_t)writeBudget/size<n)n=(size_t)writeBudget/size;
 result=fwrite(p,size,n,f);writes++;lastOutput=f;if(writeBudget>=0)writeBudget-=(long)(size*result);return result;
}
static int checkedClose(FILE *f){int fail=failClose&&f==lastOutput,result;if(f==lastOutput)lastOutput=NULL;result=fclose(f);return fail?EOF:result;}
#define fwrite checkedWrite
#define fclose checkedClose
#define main unusedConsoleMain
#include "console.c"
#undef main
#undef fwrite
#undef fclose
static unsigned live,calls,failAllocation;
static void *trackedAlloc(ISzAllocPtr unused,size_t size){void *p;(void)unused;if(++calls==failAllocation)return NULL;p=malloc(size?size:1);if(p)live++;return p;}
static void trackedFree(ISzAllocPtr unused,void *p){(void)unused;if(p){live--;free(p);}}
static int cancelOutput(UInt64 position){(void)position;return !writes;}
int main(int argc,char **argv){
 Input input;CSzArEx archive;CLookToRead2 look;ISzAlloc alloc={trackedAlloc,trackedFree};Byte buffer[4096],*mask=NULL;size_t bytes;unsigned baseline;SRes result;char *p,*end;
 if(argc!=7)return 7;
 CrcGenerateTable();memset(&input,0,sizeof(input));input.iface.Read=readInput;input.iface.Seek=seekInput;input.file=fopen(argv[1],"rb");if(!input.file)return 8;
 LookToRead2_CreateVTable(&look,False);look.realStream=&input.iface;look.buf=buffer;look.bufSize=sizeof(buffer);LookToRead2_INIT(&look);SzArEx_Init(&archive);
 result=openArchive(&archive,&input,&look,&alloc);if(result!=SZ_OK)return 9;
 bytes=(size_t)(archive.NumFiles/8+(archive.NumFiles%8!=0));mask=(Byte *)calloc(bytes?bytes:1,1);if(!mask)return 10;
 p=argv[4];while(*p&&strcmp(p,"none")&&strcmp(p,"short")&&strcmp(p,"all")){
  unsigned long index=strtoul(p,&end,10);if(end==p||index>=archive.NumFiles||(*end&&*end!=','))return 11;
  mask[index/8]|=1u<<(index%8);p=*end?end+1:end;
 }
 baseline=live;calls=0;failAllocation=(unsigned)strtoul(argv[5],NULL,10);
 if(!strcmp(argv[6],"write"))writeBudget=5000;
 if(!strcmp(argv[6],"close"))failClose=1;
 if(!strcmp(argv[6],"cancel"))testProgress=cancelOutput;
 result=extractArchiveSelected(&archive,&input,&look,&alloc,argv[2],atoi(argv[3]),!strcmp(argv[4],"all")?NULL:mask,!strcmp(argv[4],"short")?0:bytes);
 testProgress=NULL;failClose=0;writeBudget=-1;
 printf("RESULT %d CALLS %u WRITES %u BASELINE %u LIVE %u\n%s\n",result,calls,writes,baseline,live,extractMessage);
 if(live!=baseline)return 12;
 SzArEx_Free(&archive,&alloc);fclose(input.file);free(mask);if(live)return 13;return 0;
}
