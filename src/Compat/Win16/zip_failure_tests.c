/* Allocation/short-read fault tests for ZIP metadata construction. */
#define main unusedConsoleMain
#include "console.c"
#undef main
static unsigned failAt,attempts,outstanding;
static void *faultAlloc(ISzAllocPtr p,size_t n){void *v;(void)p;if(++attempts==failAt)return NULL;if(n>65520U)return NULL;v=malloc(n);if(v)outstanding++;return v;}
static void faultFree(ISzAllocPtr p,void *v){(void)p;if(v){outstanding--;free(v);}}
static SRes shortRead(ISeekInStreamPtr p,void *data,size_t *n){if(*n>7)*n=7;return readInput(p,data,n);}
int main(int argc,char **argv){
 unsigned index,total=0;Input in;CSzArEx a;ISzAlloc alloc={faultAlloc,faultFree};SRes result;
 if(argc!=2)return 7;in.iface.Read=shortRead;in.iface.Seek=seekInput;in.file=fopen(argv[1],"rb");if(!in.file)return 2;
 for(index=0;index<=total;index++){
  unsigned extra=0;attempts=outstanding=0;failAt=index;SzArEx_Init(&a);
  result=Z7Win16_OpenZip(&a,&in.iface,&alloc,&extra);
  if(!index){if(result!=SZ_OK)return 2;total=attempts;}
  else if(result!=SZ_ERROR_MEM){printf("FAIL allocation %u result %d\n",index,result);return 2;}
  SzArEx_Free(&a,&alloc);
  if(outstanding){printf("FAIL allocation %u leaked %u blocks\n",index,outstanding);return 2;}
 }
 fclose(in.file);printf("PASS all %u allocation failures, seven-byte reads and metadata cleanup\n",total);return 0;
}
