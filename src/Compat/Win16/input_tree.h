/* Bounded, iterative source enumeration shared by native archive writers.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_INPUT_TREE_H
#define Z7_WIN16_INPUT_TREE_H
#ifdef __WATCOMC__
#include <dos.h>
#else
#include <dirent.h>
#endif
#define INPUT_TREE_MAX 4096
typedef struct {char *source,*name;Byte directory;} InputNode;
typedef struct {InputNode *items;unsigned count,capacity;} InputTree;
static char inputTreeMessage[320];
static int inputTreeWideMatch(const Byte *wide,const char *name){
 unsigned i=0;for(;;){unsigned a=wide[2*i]|((unsigned)wide[2*i+1]<<8),b=(unsigned char)name[i];
  if(a=='\\')a='/';if(b=='\\')b='/';if(foldName(a)!=foldName(b))return 0;if(!a)return 1;i++;
 }
}
static void inputTreeFree(InputTree *tree,ISzAllocPtr alloc){
 unsigned i;for(i=0;i<tree->count;i++)ISzAlloc_Free(alloc,tree->items[i].source);
 ISzAlloc_Free(alloc,tree->items);memset(tree,0,sizeof(*tree));
}
static int inputTreeSameOutput(const char *source,const char *destination,const struct stat *st){
#ifdef __WATCOMC__
 char a[260],b[260];(void)st;
 if(!destination)return 0;
 return _fullpath(a,source,sizeof(a))&&_fullpath(b,destination,sizeof(b))&&sameName(a,b);
#else
 struct stat output;return destination&&!stat(destination,&output)&&st->st_dev==output.st_dev&&st->st_ino==output.st_ino;
#endif
}
static SRes inputTreeAdd(InputTree *tree,const char *source,const char *name,const char *destination,ISzAllocPtr alloc){
 struct stat st;unsigned i;size_t sn=strlen(source),nn=strlen(name);InputNode *node;
 if(testProgress&&!testProgress(0))return SZ_ERROR_PROGRESS;
 if(sn>=260||nn>=255)return SZ_ERROR_UNSUPPORTED;
#ifdef __WATCOMC__
 if(!validDosDestination(source))return SZ_ERROR_UNSUPPORTED;
 if(stat(source,&st))return SZ_ERROR_READ;
#else
 if(lstat(source,&st))return SZ_ERROR_READ;
#endif
 if((st.st_mode&S_IFMT)!=S_IFREG&&(st.st_mode&S_IFMT)!=S_IFDIR){
  strcpy(inputTreeMessage,"ERROR: A source is not a regular file or directory. Links and special files are not followed.");return SZ_ERROR_UNSUPPORTED;
 }
 /* Enumerate before making any temporary output. Existing archive aliases
    are excluded as well, so an update never archives itself. */
 if(inputTreeSameOutput(source,destination,&st))return SZ_OK;
 for(i=0;i<tree->count;i++)if(sameName(tree->items[i].name,name)){
  strcpy(inputTreeMessage,"ERROR: Source names collide on a DOS filesystem.");return SZ_ERROR_UNSUPPORTED;
 }
 if(tree->count==INPUT_TREE_MAX){strcpy(inputTreeMessage,"ERROR: This native operation currently supports at most 4096 file and directory entries (also bounded by conventional memory).");return SZ_ERROR_UNSUPPORTED;}
 /* Keep small source trees small. Reserving all 256 nodes up front consumes
    conventional memory needed by simultaneous decoder/encoder updates. Names
    have separate ownership, so growing the node array never moves them. */
 if(tree->count==tree->capacity){
  unsigned capacity=tree->capacity?tree->capacity*2:8;
  InputNode *items=(InputNode *)ISzAlloc_Alloc(alloc,capacity*sizeof(InputNode));
  if(!items)return SZ_ERROR_MEM;
  if(tree->count)memcpy(items,tree->items,tree->count*sizeof(InputNode));
  ISzAlloc_Free(alloc,tree->items);tree->items=items;tree->capacity=capacity;
 }
 node=tree->items+tree->count;
 node->source=(char *)ISzAlloc_Alloc(alloc,sn+nn+2);if(!node->source)return SZ_ERROR_MEM;
 strcpy(node->source,source);node->name=node->source+sn+1;strcpy(node->name,name);
 node->directory=(st.st_mode&S_IFMT)==S_IFDIR;tree->count++;return SZ_OK;
}
static SRes inputTreeChild(InputTree *tree,const char *parent,const char *prefix,const char *child,const char *destination,ISzAllocPtr alloc){
 char source[260],name[260];size_t pn=strlen(parent),nn=strlen(prefix),cn=strlen(child);
 if(!strcmp(child,".")||!strcmp(child,".."))return SZ_OK;
 if(!validDosComponent(child,(unsigned)cn)){
  strcpy(inputTreeMessage,"ERROR: Source names must fit DOS 8.3 without conversion.");return SZ_ERROR_UNSUPPORTED;
 }
 if(pn+cn+2>sizeof(source)||nn+cn+2>sizeof(name))return SZ_ERROR_UNSUPPORTED;
 strcpy(source,parent);if(pn&&source[pn-1]!='/'&&source[pn-1]!='\\')source[pn++]=Z7_PATH_SEPARATOR;strcpy(source+pn,child);
 strcpy(name,prefix);if(nn)name[nn++]='/';strcpy(name+nn,child);
 return inputTreeAdd(tree,source,name,destination,alloc);
}
static SRes inputTreeDirectory(InputTree *tree,const char *source,const char *prefix,const char *destination,ISzAllocPtr alloc){
 SRes result=SZ_OK;
#ifdef __WATCOMC__
 struct find_t entry;char pattern[260];size_t n=strlen(source);unsigned code;
 if(n+5>sizeof(pattern))return SZ_ERROR_UNSUPPORTED;
 strcpy(pattern,source);if(n&&pattern[n-1]!='/'&&pattern[n-1]!='\\')pattern[n++]=Z7_PATH_SEPARATOR;strcpy(pattern+n,"*.*");
 code=_dos_findfirst(pattern,_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_SUBDIR|_A_ARCH,&entry);
 while(!code){result=inputTreeChild(tree,source,prefix,entry.name,destination,alloc);if(result!=SZ_OK)return result;code=_dos_findnext(&entry);}
 if(code!=18&&code!=2)return SZ_ERROR_READ;
#else
 DIR *dir=opendir(source);struct dirent *entry;if(!dir)return SZ_ERROR_READ;
 for(;;){errno=0;entry=readdir(dir);if(!entry){if(errno)result=SZ_ERROR_READ;break;}
  result=inputTreeChild(tree,source,prefix,entry->d_name,destination,alloc);if(result!=SZ_OK)break;
 }
 if(closedir(dir)&&result==SZ_OK)result=SZ_ERROR_READ;
#endif
 return result;
}
static SRes inputTreeBuild(InputTree *tree,unsigned count,const char * const *sources,const char *destination,ISzAllocPtr alloc){
 unsigned i;SRes result;inputTreeMessage[0]=0;memset(tree,0,sizeof(*tree));
 if(!count||count>INPUT_TREE_MAX)return SZ_ERROR_UNSUPPORTED;
 for(i=0;i<count;i++){
  char source[260];const char *name,*p;size_t n=strlen(sources[i]);
  if(!n||n>=sizeof(source))return SZ_ERROR_UNSUPPORTED;strcpy(source,sources[i]);
  while(n>1&&(source[n-1]=='/'||source[n-1]=='\\')&&source[n-2]!=':')source[--n]=0;
  name=source;for(p=source;*p;p++)if(*p=='/'||*p=='\\'||*p==':')name=p+1;
  /* A dot or drive/root source means its contents, with no synthetic entry. */
  if(!*name||!strcmp(name,".")||!strcmp(name,".."))result=inputTreeDirectory(tree,source,"",destination,alloc);
  else if(!validDosComponent(name,(unsigned)strlen(name))){strcpy(inputTreeMessage,"ERROR: Source names must fit DOS 8.3 without conversion.");result=SZ_ERROR_UNSUPPORTED;}
  else result=inputTreeAdd(tree,source,name,destination,alloc);
  if(result!=SZ_OK)return result;
 }
 /* Breadth-first enumeration avoids unbounded C stack use in real mode. */
 for(i=0;i<tree->count;i++)if(tree->items[i].directory){
  result=inputTreeDirectory(tree,tree->items[i].source,tree->items[i].name,destination,alloc);if(result!=SZ_OK)return result;
 }
 return tree->count?SZ_OK:SZ_ERROR_UNSUPPORTED;
}
#endif
