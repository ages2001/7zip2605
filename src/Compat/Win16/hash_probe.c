/* Native checksum fixture; the production frontends share hash_file.c. */
#include <stdio.h>
#include <stdlib.h>
#include "hash_file.h"
static void *a(ISzAllocPtr p,size_t n){(void)p;return malloc(n);}
static void f(ISzAllocPtr p,void *v){(void)p;free(v);}
int main(int argc,char **argv){ISzAlloc alloc={a,f};unsigned i,j;Byte digest[64];UInt64 size;SRes result;if(argc!=2)return 7;
 for(i=0;i<Z7_WIN16_HASH_COUNT;i++){
  result=Z7Win16_HashFile(argv[1],i,digest,&size,&alloc,NULL);if(result!=SZ_OK){printf("ERROR %s %d\n",Z7Win16_HashMethods[i].name,result);return 2;}
  printf("%s ",Z7Win16_HashMethods[i].name);for(j=0;j<Z7Win16_HashMethods[i].digestSize;j++)printf("%02X",(unsigned)digest[j]);puts("");
 }
 return 0;
}
