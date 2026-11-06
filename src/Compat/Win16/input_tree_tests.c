/* Source-plan growth and allocation ownership on host and native 8086 DOS.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#define main unusedConsoleMain
#include "console.c"
#undef main
static unsigned attempts, failAt, live;
static void *planAlloc(ISzAllocPtr unused,size_t size){
 void *p;(void)unused;if(++attempts==failAt)return NULL;
 p=malloc(size);if(p)++live;return p;
}
static void planFree(ISzAllocPtr unused,void *p){(void)unused;if(p){--live;free(p);}}
static int checkPlan(const char *source,unsigned failure,unsigned *total){
 ISzAlloc alloc={planAlloc,planFree};InputTree tree;unsigned i,j,previousCapacity;SRes result=SZ_OK;
 char name[16];int valid=1;
 memset(&tree,0,sizeof(tree));attempts=0;failAt=failure;
 for(i=0;i<256;i++){
  previousCapacity=tree.capacity;
  sprintf(name,"F%03u.BIN",i);result=inputTreeAdd(&tree,source,name,NULL,&alloc);
  if(result!=SZ_OK)break;
  if(tree.count!=i+1||tree.capacity<tree.count||tree.capacity>256)valid=0;
  if(i<8&&tree.capacity!=8)valid=0;
  /* Recheck every retained name when its node array moves, plus the final
     complete array. Between growth boundaries only the new node can change.
     This keeps the exhaustive failure loop practical on native DOS. */
  for(j=tree.capacity!=previousCapacity||i==255?0:i;j<=i;j++){
   sprintf(name,"F%03u.BIN",j);
   if(strcmp(tree.items[j].name,name)||strcmp(tree.items[j].source,source))valid=0;
  }
 }
 if(!failure){
  valid&=result==SZ_OK&&tree.count==256&&tree.capacity==256;
  *total=attempts;
  valid&=inputTreeAdd(&tree,source,"EXTRA.BIN",NULL,&alloc)==SZ_ERROR_UNSUPPORTED;
  valid&=tree.count==256;
 }else valid&=result==SZ_ERROR_MEM;
 inputTreeFree(&tree,&alloc);valid&=!live&&!tree.items&&!tree.count&&!tree.capacity;
 if(!valid)printf("FAIL source-plan growth allocation=%u result=%d live=%u\n",failure,result,live);
 return valid;
}
int main(int argc,char **argv){
 unsigned i,total=0;
 if(argc!=2)return 7;
 if(!checkPlan(argv[1],0,&total))return 2;
 printf("PASS source-plan growth baseline; checking %u allocation failures\n",total);fflush(stdout);
 for(i=1;i<=total;i++){
  if(!checkPlan(argv[1],i,&total))return 2;
  if(!(i&63)){printf("PASS source-plan allocation failures 1..%u\n",i);fflush(stdout);}
 }
 printf("PASS source-plan growth 8..256, names preserved, entry limit and all %u allocation failures; no leaks\n",total);
 return 0;
}
