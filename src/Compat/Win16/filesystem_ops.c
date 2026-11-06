/* Transactional creation of new file/folder copies; native DOS filesystem work.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __WATCOMC__
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <utime.h>
#ifdef __WATCOMC__
#include <dos.h>
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#include <sys/syscall.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
#endif
#include "filesystem_ops.h"
#include "dos_paths.h"
#include "../../C/7zCrc.h"
static Z7FsProgress testProgress;
#include "input_tree.h"

typedef struct {UInt32 crc,size;time_t modified;} FsIdentity;
typedef struct {FsIdentity content;unsigned attributes;} FsReplacement;
static int fsStat(const char *path,struct stat *info){
#ifdef __WATCOMC__
 return stat(path,info);
#else
 return lstat(path,info);
#endif
}
static int fsCanonical(const char *input,char *out){
 size_t n;char *base;
 if(!input||!*input||strlen(input)>=260)return 0;
#ifdef __WATCOMC__
 if(!_fullpath(out,input,260))return 0;
 n=strlen(out);while(n>3&&(out[n-1]=='\\'||out[n-1]=='/'))out[--n]=0;
 if(n<=3||out[1]!=':'||!validDosDestination(out))return 0;
#else
 {
  char parent[260],leaf[260],*resolved,*slash;
  strcpy(parent,input);n=strlen(parent);while(n>1&&parent[n-1]=='/')parent[--n]=0;
  slash=strrchr(parent,'/');
  if(slash){strcpy(leaf,slash+1);if(slash==parent)slash[1]=0;else *slash=0;}
  else{strcpy(leaf,parent);strcpy(parent,".");}
  if(!validDosComponent(leaf,(unsigned)strlen(leaf)))return 0;
  resolved=realpath(parent,NULL);if(!resolved)return 0;n=strlen(resolved);
  if(n+strlen(leaf)+2>=260){free(resolved);return 0;}
  strcpy(out,resolved);free(resolved);if(out[n-1]!='/')out[n++]='/';strcpy(out+n,leaf);
 }
#endif
 base=out;for(n=0;out[n];n++)if(out[n]=='/'||out[n]=='\\'||out[n]==':')base=out+n+1;
 return validDosComponent(base,(unsigned)strlen(base));
}
static int fsRenameNew(const char *source,const char *destination){
#ifdef __WATCOMC__
 /* DOS refuses a pre-existing destination, including directories. */
 return rename(source,destination);
#elif defined(SYS_renameat2)
 return (int)syscall(SYS_renameat2,AT_FDCWD,source,AT_FDCWD,destination,1u);
#else
 errno=ENOSYS;return -1;
#endif
}
static int fsPath(const InputTree *tree,unsigned index,const char *root,char *out){
 const char *suffix=tree->items[index].name+strlen(tree->items[0].name);size_t n=strlen(root),i;
 if(n+strlen(suffix)>=260)return 0;
 strcpy(out,root);strcpy(out+n,suffix);
 for(i=n;out[i];i++)if(out[i]=='/'||out[i]=='\\')out[i]=Z7_PATH_SEPARATOR;
 return 1;
}
static int fsWritable(const char *path){
#ifdef __WATCOMC__
 unsigned attributes;
 return !_dos_getfileattr(path,&attributes)&&!_dos_setfileattr(path,(attributes&0x27)&~_A_RDONLY);
#else
 struct stat info;return !lstat(path,&info)&&!chmod(path,info.st_mode|S_IWUSR);
#endif
}
static int fsRemoveOwned(const char *path,int directory){
 int result;
#ifdef __WATCOMC__
 unsigned attributes;
 if(_dos_getfileattr(path,&attributes))return 0;
 /* The DOS C runtime may refuse hidden/system names as well as readonly
    files. Clear these only for the explicitly selected removal, and restore
    the exact original attributes if removal fails. */
 if((attributes&(_A_RDONLY|_A_HIDDEN|_A_SYSTEM))&&_dos_setfileattr(path,attributes&_A_ARCH))return 0;
 result=directory?rmdir(path):remove(path);
 if(result)_dos_setfileattr(path,attributes&0x27);
#else
 struct stat info;
 if(lstat(path,&info)||!fsWritable(path))return 0;
 result=directory?rmdir(path):remove(path);
 if(result)chmod(path,info.st_mode&0777);
#endif
 return result==0;
}
static int fsRollback(const InputTree *tree,unsigned created,const char *root){
 char path[260];int ok=1;
 {unsigned i;for(i=0;i<created;i++)if(tree->items[i].directory&&fsPath(tree,i,root,path)&&!fsWritable(path))ok=0;}
 while(created){unsigned index=--created;
  if(!fsPath(tree,index,root,path)||!fsRemoveOwned(path,tree->items[index].directory))ok=0;
 }
 return ok;
}
static int fsSibling(const char *destination,char *path,unsigned attempt){
 const char *base=destination,*p;size_t prefix;
 for(p=destination;*p;p++)if(*p=='/'||*p=='\\'||*p==':')base=p+1;
 prefix=(size_t)(base-destination);if(prefix+13>=260)return 0;
 memcpy(path,destination,prefix);sprintf(path+prefix,"7F%06X.CPY",attempt);return 1;
}
static const char *fsLeaf(const char *path){
 const char *leaf=path;for(;*path;path++)if(*path=='/'||*path=='\\'||*path==':')leaf=path+1;return leaf;
}
/* Snapshot the existing destination before copying, then recheck its contents
   and representable metadata before publishing the replacement. */
static SRes fsReplacementIdentity(const char *path,Byte *buffer,FsReplacement *identity,UInt64 progressBytes){
 FILE *file;struct stat named,before,after;size_t n;UInt32 crc=CRC_INIT_VAL,size=0;SRes result=SZ_OK;
#ifdef __WATCOMC__
 unsigned attributes;
#endif
 if(fsStat(path,&named)||(named.st_mode&S_IFMT)!=S_IFREG)return SZ_ERROR_READ;
#ifdef __WATCOMC__
 file=_fsopen(path,"rb",SH_DENYWR);
#else
 file=fopen(path,"rb");
#endif
 if(!file)return SZ_ERROR_READ;
 if(fstat(fileno(file),&before)||(before.st_mode&S_IFMT)!=S_IFREG||before.st_size<0){result=SZ_ERROR_READ;goto done;}
#ifndef __WATCOMC__
 if(named.st_dev!=before.st_dev||named.st_ino!=before.st_ino){result=SZ_ERROR_READ;goto done;}
#endif
#ifdef __WATCOMC__
 if(_dos_getfileattr(path,&attributes)){result=SZ_ERROR_READ;goto done;}
 identity->attributes=attributes&0x27;
#else
 identity->attributes=(unsigned)before.st_mode&0777;
#endif
 for(;;){
  if(testProgress&&!testProgress(progressBytes)){result=SZ_ERROR_PROGRESS;goto done;}
  n=fread(buffer,1,4096,file);if(!n)break;
  if((UInt32)n>0xffffffffUL-size){result=SZ_ERROR_UNSUPPORTED;goto done;}
  crc=CrcUpdate(crc,buffer,n);size+=(UInt32)n;
 }
 if(ferror(file)||fstat(fileno(file),&after)||before.st_size!=after.st_size||before.st_mtime!=after.st_mtime||(UInt64)size!=(UInt64)before.st_size){result=SZ_ERROR_READ;goto done;}
 if(fsStat(path,&named)||(named.st_mode&S_IFMT)!=S_IFREG){result=SZ_ERROR_READ;goto done;}
#ifdef __WATCOMC__
 if(_dos_getfileattr(path,&attributes)||(attributes&0x27)!=identity->attributes){result=SZ_ERROR_READ;goto done;}
#else
 if(named.st_dev!=before.st_dev||named.st_ino!=before.st_ino||((unsigned)after.st_mode&0777)!=identity->attributes){result=SZ_ERROR_READ;goto done;}
#endif
 identity->content.crc=CRC_GET_DIGEST(crc);identity->content.size=size;identity->content.modified=before.st_mtime;
done:
 if(fclose(file)&&result==SZ_OK)result=SZ_ERROR_READ;return result;
}
static int fsSameReplacement(const FsReplacement *a,const FsReplacement *b){
 return a->content.crc==b->content.crc&&a->content.size==b->content.size&&a->content.modified==b->content.modified&&a->attributes==b->attributes;
}
static SRes fsCopyFile(const char *source,const char *destination,Byte *buffer,
    FsIdentity *identity,Z7FsReport *report,int *created){
 FILE *input=NULL,*output=NULL;struct stat before,after;int fd=-1;SRes result=SZ_OK;size_t n;UInt32 crc=CRC_INIT_VAL,size=0;
#ifdef __WATCOMC__
 unsigned date,time,attributes;
 input=_fsopen(source,"rb",SH_DENYWR);
#else
 input=fopen(source,"rb");
#endif
 if(!input||fstat(fileno(input),&before)||before.st_size<0||(before.st_mode&S_IFMT)!=S_IFREG){result=SZ_ERROR_READ;goto done;}
#ifdef __WATCOMC__
 if(_dos_getftime(fileno(input),&date,&time)||_dos_getfileattr(source,&attributes)){result=SZ_ERROR_READ;goto done;}
#endif
 fd=open(destination,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);
 if(fd<0){result=SZ_ERROR_WRITE;goto done;}*created=1;
 output=fdopen(fd,"wb");if(!output){close(fd);result=SZ_ERROR_WRITE;goto done;}
 for(;;){
  if(testProgress&&!testProgress(report->bytes)){result=SZ_ERROR_PROGRESS;break;}
  n=fread(buffer,1,4096,input);if(!n){if(ferror(input))result=SZ_ERROR_READ;break;}
  if((UInt32)n>0xffffffffUL-size){result=SZ_ERROR_UNSUPPORTED;break;}
  if(fwrite(buffer,1,n,output)!=n){result=SZ_ERROR_WRITE;break;}
  crc=CrcUpdate(crc,buffer,n);size+=(UInt32)n;report->bytes+=n;
 }
 if(result==SZ_OK&&(fstat(fileno(input),&after)||after.st_size!=before.st_size||after.st_mtime!=before.st_mtime||(UInt64)size!=(UInt64)before.st_size))result=SZ_ERROR_READ;
 if(result==SZ_OK&&fflush(output))result=SZ_ERROR_WRITE;
#ifdef __WATCOMC__
 if(result==SZ_OK&&_dos_setftime(fileno(output),date,time))result=SZ_ERROR_WRITE;
#endif
done:
 if(input&&fclose(input)&&result==SZ_OK)result=SZ_ERROR_READ;
 if(output&&fclose(output)&&result==SZ_OK)result=SZ_ERROR_WRITE;
 if(result==SZ_OK){
#ifdef __WATCOMC__
  if(_dos_setfileattr(destination,attributes&0x27))result=SZ_ERROR_WRITE;
#else
  struct utimbuf times;times.actime=before.st_atime;times.modtime=before.st_mtime;
  if(utime(destination,&times)||chmod(destination,before.st_mode&0777))result=SZ_ERROR_WRITE;
#endif
  identity->crc=CRC_GET_DIGEST(crc);identity->size=size;identity->modified=before.st_mtime;
 }
 return result;
}
static SRes fsDirectoryMetadata(const char *source,const char *destination,int *timeMissing){
 struct stat info;struct utimbuf times;
 if(fsStat(source,&info))return SZ_ERROR_READ;
 times.actime=info.st_atime;times.modtime=info.st_mtime;
 if(utime(destination,&times))*timeMissing=1;
#ifdef __WATCOMC__
 {unsigned attributes;if(_dos_getfileattr(source,&attributes)||_dos_setfileattr(destination,attributes&0x27))return SZ_ERROR_WRITE;}
#else
 if(chmod(destination,info.st_mode&0777))return SZ_ERROR_WRITE;
#endif
 return SZ_OK;
}
static SRes fsVerifySource(const InputTree *tree,const FsIdentity *identities,Byte *buffer){
 unsigned i;
 for(i=0;i<tree->count;i++)if(!tree->items[i].directory){
  FILE *file;struct stat info;size_t n;UInt32 crc=CRC_INIT_VAL,size=0;int ok=1;
  if(fsStat(tree->items[i].source,&info)||info.st_size<0||(UInt64)info.st_size!=identities[i].size||info.st_mtime!=identities[i].modified)return SZ_ERROR_READ;
  file=fopen(tree->items[i].source,"rb");if(!file)return SZ_ERROR_READ;
  while((n=fread(buffer,1,4096,file))!=0){if((UInt32)n>0xffffffffUL-size){ok=0;break;}crc=CrcUpdate(crc,buffer,n);size+=(UInt32)n;}
  if(ferror(file))ok=0;if(fclose(file))ok=0;
  if(!ok||size!=identities[i].size||CRC_GET_DIGEST(crc)!=identities[i].crc)return SZ_ERROR_READ;
 }
 return SZ_OK;
}
SRes Z7Win16_FileOperation(unsigned operation,const char *source,const char *destination,
    ISzAllocPtr allocator,Z7FsProgress progress,Z7FsReport *report){
 char src[260],dest[260],stage[260],path[260],backup[260];struct stat info;InputTree tree;FsIdentity *identities=NULL;Byte *buffer=NULL;FsReplacement original,current;
 unsigned i,attempt,created=0;SRes result=SZ_OK;int rootDirectory=0,owned=0,committed=0,timeMissing=0;const char *sources[1];
 int replace=operation==Z7_FS_COPY_REPLACE||operation==Z7_FS_MOVE_REPLACE,existing=0,backupOwned=0;
 if(!report||!allocator||!allocator->Alloc||!allocator->Free)return SZ_ERROR_PARAM;
 memset(report,0,sizeof(*report));memset(&tree,0,sizeof(tree));stage[0]=backup[0]=0;testProgress=progress;
 if(operation==Z7_FS_COPY_REPLACE)operation=Z7_FS_COPY;
 else if(operation==Z7_FS_MOVE_REPLACE)operation=Z7_FS_MOVE;
 if(operation<Z7_FS_COPY||operation>Z7_FS_MKDIR){result=SZ_ERROR_PARAM;goto done;}
 if(operation!=Z7_FS_DELETE&&!fsCanonical(destination,dest)){result=SZ_ERROR_UNSUPPORTED;goto done;}
 if(operation!=Z7_FS_DELETE){
  if(!fsStat(dest,&info)){
   if(!replace||(info.st_mode&S_IFMT)!=S_IFREG){strcpy(report->message,"ERROR: The destination already exists. Choose a new name; no existing file was replaced.");result=SZ_ERROR_WRITE;goto done;}
   existing=1;
  }else if(errno!=ENOENT){result=SZ_ERROR_WRITE;goto done;}
 }
 if(operation==Z7_FS_MKDIR){if(Z7_MKDIR(dest))result=SZ_ERROR_WRITE;else report->folders=1;goto done;}
 if(!fsCanonical(source,src)||fsStat(src,&info)||((info.st_mode&S_IFMT)!=S_IFDIR&&(info.st_mode&S_IFMT)!=S_IFREG)){result=SZ_ERROR_UNSUPPORTED;goto done;}
 rootDirectory=(info.st_mode&S_IFMT)==S_IFDIR;
 if(existing&&rootDirectory){strcpy(report->message,"ERROR: A file cannot be replaced by a folder.");result=SZ_ERROR_UNSUPPORTED;goto done;}
 if(operation!=Z7_FS_DELETE&&(sameName(src,dest)||(rootDirectory&&namePrefix(src,dest)))){strcpy(report->message,"ERROR: A folder cannot be copied or moved into itself.");result=SZ_ERROR_PARAM;goto done;}
 if(progress&&!progress(0)){result=SZ_ERROR_PROGRESS;goto done;}
 if(operation==Z7_FS_RENAME||(operation==Z7_FS_MOVE&&!existing)){
  if(!fsRenameNew(src,dest)){if(rootDirectory)report->folders=1;else report->files=1;goto done;}
  if(operation==Z7_FS_RENAME||errno!=EXDEV){result=SZ_ERROR_WRITE;goto done;}
 }
 sources[0]=src;result=inputTreeBuild(&tree,1,sources,NULL,allocator);if(result!=SZ_OK){if(inputTreeMessage[0])strcpy(report->message,inputTreeMessage);goto done;}
 if(operation==Z7_FS_DELETE){
  for(i=tree.count;i>0;i--){
   InputNode *node=tree.items+i-1;
   if(progress&&!progress(report->files+report->folders)){result=SZ_ERROR_PROGRESS;break;}
   if(!fsRemoveOwned(node->source,node->directory)){result=SZ_ERROR_WRITE;sprintf(report->message,"ERROR: Cannot remove %s (DOS error %d).",node->source,errno);break;}
   if(node->directory)report->folders++;else report->files++;
  }
  if(result!=SZ_OK&&!report->message[0])sprintf(report->message,"Deletion stopped after %lu files and %lu folders. Remaining source items were not removed.",(unsigned long)report->files,(unsigned long)report->folders);
  goto done;
 }
 /* Validate every destination length before creating any staged output. */
 for(i=0;i<tree.count;i++)if(!fsPath(&tree,i,dest,path)){result=SZ_ERROR_UNSUPPORTED;goto done;}
 buffer=(Byte *)ISzAlloc_Alloc(allocator,4096);identities=(FsIdentity *)ISzAlloc_Alloc(allocator,tree.count*sizeof(FsIdentity));
 if(!buffer||!identities){result=SZ_ERROR_MEM;goto done;}
 memset(identities,0,tree.count*sizeof(FsIdentity));
 if(existing){result=fsReplacementIdentity(dest,buffer,&original,report->bytes);if(result!=SZ_OK)goto done;}
 for(attempt=0;attempt<4096;attempt++){
  if(!fsSibling(dest,stage,attempt)){result=SZ_ERROR_UNSUPPORTED;goto done;}
  if(rootDirectory){if(!Z7_MKDIR(stage)){created=1;owned=1;break;}}
  else{if(fsStat(stage,&info)&&errno==ENOENT){owned=1;break;}errno=EEXIST;}
  if(errno!=EEXIST){result=SZ_ERROR_WRITE;goto done;}
 }
 if(attempt==4096){result=SZ_ERROR_WRITE;goto done;}
 for(i=0;i<tree.count;i++){
  InputNode *node=tree.items+i;int made=0;
  if(!fsPath(&tree,i,stage,path)){result=SZ_ERROR_UNSUPPORTED;goto done;}
  if(progress&&!progress(report->bytes)){result=SZ_ERROR_PROGRESS;goto done;}
  if(node->directory){if(i&&Z7_MKDIR(path)){result=SZ_ERROR_WRITE;goto done;}created=i+1;report->folders++;}
  else{result=fsCopyFile(node->source,path,buffer,identities+i,report,&made);if(made)created=i+1;if(result!=SZ_OK)goto done;report->files++;}
 }
 for(i=tree.count;i>0;i--)if(tree.items[i-1].directory){
  if(!fsPath(&tree,i-1,stage,path)){result=SZ_ERROR_UNSUPPORTED;goto done;}
  result=fsDirectoryMetadata(tree.items[i-1].source,path,&timeMissing);if(result!=SZ_OK)goto done;
 }
 if(timeMissing&&operation==Z7_FS_MOVE){result=SZ_ERROR_UNSUPPORTED;strcpy(report->message,"ERROR: Folder timestamps cannot be preserved for this cross-drive move. The source remains intact; use Copy instead.");goto done;}
 if(operation==Z7_FS_MOVE){result=fsVerifySource(&tree,identities,buffer);if(result!=SZ_OK){strcpy(report->message,"ERROR: Source files changed during the move. The source remains intact.");goto done;}}
 if(progress&&!progress(report->bytes)){result=SZ_ERROR_PROGRESS;goto done;}
 if(existing){
  result=fsReplacementIdentity(dest,buffer,&current,report->bytes);if(result!=SZ_OK)goto done;
  if(!fsSameReplacement(&original,&current)){result=SZ_ERROR_WRITE;strcpy(report->message,"ERROR: The destination changed during copying. Its current contents were preserved.");goto done;}
  for(attempt=0;attempt<4096;attempt++){
   if(!fsSibling(dest,backup,attempt)){result=SZ_ERROR_UNSUPPORTED;goto done;}
   strcpy(backup+strlen(backup)-3,"BAK");
   if(sameName(backup,dest))continue;
   if(!fsStat(backup,&info))continue;
   if(errno!=ENOENT){result=SZ_ERROR_WRITE;goto done;}
   if(!fsRenameNew(dest,backup)){backupOwned=1;break;}
   if(errno!=EEXIST){result=SZ_ERROR_WRITE;goto done;}
  }
  if(!backupOwned){result=SZ_ERROR_WRITE;goto done;}
  /* Revalidate the actual file moved aside, so a changed destination is never
     discarded merely because it passed an earlier pathname check. */
  result=fsReplacementIdentity(backup,buffer,&current,report->bytes);if(result!=SZ_OK)goto done;
  if(!fsSameReplacement(&original,&current)){result=SZ_ERROR_WRITE;strcpy(report->message,"ERROR: The destination changed before replacement. Its contents were preserved.");goto done;}
 }
 if(fsRenameNew(stage,dest)){result=SZ_ERROR_WRITE;goto done;}owned=0;committed=1;
 if(backupOwned){
  if(fsRemoveOwned(backup,0))backupOwned=0;
  else{result=SZ_ERROR_WRITE;sprintf(report->message,operation==Z7_FS_MOVE?"ERROR: Old file retained at %s; source kept.":"ERROR: Replacement complete; previous file remains at %s.",backup);}
 }
 if(operation==Z7_FS_MOVE&&result==SZ_OK){
  /* Destination validation can pump messages after the earlier source check.
     Never delete a source edited while the completed copy was published. */
  result=fsVerifySource(&tree,identities,buffer);
  if(result!=SZ_OK){strcpy(report->message,"ERROR: The destination copy is complete, but source verification failed. The source was preserved.");goto done;}
  for(i=tree.count;i>0;i--)if(!fsRemoveOwned(tree.items[i-1].source,tree.items[i-1].directory)){result=SZ_ERROR_WRITE;break;}
  if(result!=SZ_OK)strcpy(report->message,"ERROR: The destination copy is complete, but some source items could not be removed.");
 }
 if(timeMissing&&result==SZ_OK)strcpy(report->message,"WARNING: Files copied, but folder timestamps could not be applied. The original folders retain their metadata.");
done:
 if(backupOwned&&!committed){
  if(!fsRenameNew(backup,dest))backupOwned=0;
  else{result=SZ_ERROR_WRITE;sprintf(report->message,"ERROR: Restore failed. Previous file saved at %s.",backup);}
 }
 if(owned&&created&&!fsRollback(&tree,created,stage)){
  result=SZ_ERROR_WRITE;
  if(backupOwned&&!committed)sprintf(report->message,"ERROR: Restore %s; partial %s; directory %.*s",fsLeaf(backup),fsLeaf(stage),(int)(fsLeaf(dest)-dest),dest);
  else sprintf(report->message,"ERROR: Cleanup failed. Partial output remains at %s.",stage);
 }
 if(result!=SZ_OK&&!committed&&operation!=Z7_FS_DELETE){report->files=0;report->folders=0;}
 inputTreeFree(&tree,allocator);ISzAlloc_Free(allocator,identities);ISzAlloc_Free(allocator,buffer);testProgress=NULL;
 if(!report->message[0])strcpy(report->message,result==SZ_OK?"Filesystem operation complete.":result==SZ_ERROR_PROGRESS?"Operation cancelled.":"ERROR: The filesystem operation could not be completed.");
 return result;
}
