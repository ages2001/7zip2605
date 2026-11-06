/* Verify hashes never report partial results after allocation/read/close/cancel errors. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned reads,allocations,failAt,live;static size_t largestRequest;static int failure;
static size_t injectedRead(void *data,size_t size,size_t count,FILE *file){reads++;if(failure==1&&reads==2)return 0;return fread(data,size,count,file);}
static int injectedClose(FILE *file){int result=fclose(file);return failure==2?EOF:result;}
#define fread injectedRead
#define fclose injectedClose
#include "hash_file.c"
#undef fclose
#undef fread
static void *allocate(ISzAllocPtr p,size_t n){void *v;(void)p;if(n>largestRequest)largestRequest=n;if(++allocations==failAt)return NULL;v=malloc(n);if(v)live++;return v;}
static void release(ISzAllocPtr p,void *v){(void)p;if(v){live--;free(v);}}
static int cancel(UInt64 pos){return !pos;}
int main(int argc,char **argv){ISzAlloc alloc={allocate,release};Byte digests[64*Z7_WIN16_HASH_COUNT];unsigned i,j;UInt64 size;SRes result;if(argc!=2)return 7;
 for(i=0;i<5;i++){
  memset(digests,0xa5,sizeof(digests));reads=allocations=0;failure=i<2?(int)i+1:0;failAt=i>=3?i-2:0;
  result=Z7Win16_HashFileMany(argv[1],(1u<<Z7_WIN16_HASH_COUNT)-1,digests,&size,&alloc,i==2?cancel:NULL);
  if(result!=(i<2?SZ_ERROR_READ:i==2?SZ_ERROR_PROGRESS:SZ_ERROR_MEM)||live||size){printf("FAIL hash error %u result=%d blocks=%u\n",i,result,live);return 2;}
  for(j=0;j<sizeof(digests);j++)if(digests[j]!=0xa5)return 2;
 }
 reads=allocations=0;failure=failAt=0;
 result=Z7Win16_HashFileMany(argv[1],(1u<<Z7_WIN16_HASH_COUNT)-1,digests,&size,&alloc,NULL);
 if(result!=SZ_OK||live||!size||largestRequest>4096||reads!=(unsigned)((size+4095)/4096+1)){printf("FAIL single read pass result=%d reads=%u\n",result,reads);return 2;}
 puts("PASS all checksum allocation, short-read, close-error and cancellation cases; no partial digest, leaks or duplicate input passes; every allocation at most 4 KiB");return 0;
}
