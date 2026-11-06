/* Real filesystem operations and injected failures, host and native DOS.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef __WATCOMC__
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <utime.h>
#ifndef __WATCOMC__
#include <unistd.h>
#include <sys/syscall.h>
#endif
static unsigned failWrite,writeCalls,failRead,readCalls,failClose,closeCalls;
static int failCommit,forceCrossMove;
static int mutateOnPublish,failSourceRemove;
static char crossSource[260];
static int failRestore,failBackupRemove,failStagingRemove,cancelBackup,changeDestination;
static char lastBackup[260];
static size_t probeWrite(const void *data,size_t size,size_t count,FILE *file){
 if(failWrite&&++writeCalls==failWrite)return fwrite(data,size,count?count-1:0,file);
 return fwrite(data,size,count,file);
}
static size_t probeRead(void *data,size_t size,size_t count,FILE *file){
 if(failRead&&++readCalls==failRead)return 0;
 return fread(data,size,count,file);
}
static int probeClose(FILE *file){int result=fclose(file);if(failClose&&++closeCalls==failClose)return EOF;return result;}
static int probeRemove(const char *path){if((failSourceRemove&&!strcmp(path,crossSource))||(failBackupRemove&&strstr(path,".BAK"))||(failStagingRemove&&strstr(path,".CPY"))){errno=EACCES;return -1;}return remove(path);}
static void afterRename(const char *source,const char *destination){
 if(strstr(destination,".BAK"))strcpy(lastBackup,destination);
 if(mutateOnPublish&&strstr(source,".CPY")){
  FILE *file;struct stat info;struct utimbuf times;
  if(stat(crossSource,&info))exit(3);times.actime=info.st_atime;times.modtime=info.st_mtime;
  file=fopen(crossSource,"r+b");if(!file)exit(3);if(fputc(99,file)==EOF||fclose(file)||utime(crossSource,&times))exit(3);
  mutateOnPublish=0;
 }
}
#ifdef __WATCOMC__
static int probeRename(const char *source,const char *destination){
 int result;
 if(forceCrossMove&&!stricmp(source,crossSource)){errno=EXDEV;return -1;}
 if(failCommit&&strstr(source,".CPY")){errno=EACCES;return -1;}
 if(failRestore&&strstr(source,".BAK")){errno=EACCES;return -1;}
 result=rename(source,destination);if(!result)afterRename(source,destination);return result;
}
#define rename probeRename
#else
static long probeSyscall(long number,...){
 va_list args;int from,to;const char *source,*destination;unsigned flags;long result;
 va_start(args,number);from=va_arg(args,int);source=va_arg(args,const char *);to=va_arg(args,int);destination=va_arg(args,const char *);flags=va_arg(args,unsigned);va_end(args);
 if(forceCrossMove&&!strcmp(source,crossSource)){errno=EXDEV;return -1;}
 if(failCommit&&strstr(source,".CPY")){errno=EACCES;return -1;}
 if(failRestore&&strstr(source,".BAK")){errno=EACCES;return -1;}
 result=syscall(number,from,source,to,destination,flags);if(!result)afterRename(source,destination);return result;
}
#define syscall probeSyscall
#endif
#define fwrite probeWrite
#define fread probeRead
#define fclose probeClose
#define remove probeRemove
#include "filesystem_ops.c"
#undef fwrite
#undef fread
#undef fclose
#undef remove
#ifdef __WATCOMC__
#undef rename
#else
#undef syscall
#endif
static unsigned allocated,allocationCalls,failAllocation,passed;
static char work[260],src[260],dst[260],extra[260],nested[260],detail[320];
static Z7FsReport report;
static UInt64 cancelAt;
static int cancelEnabled,collisionEnabled,mutationEnabled;
static void *trackedAlloc(ISzAllocPtr unused,size_t size){void *p;(void)unused;allocationCalls++;if(failAllocation==allocationCalls)return NULL;p=malloc(size?size:1);if(p)allocated++;return p;}
static void trackedFree(ISzAllocPtr unused,void *p){(void)unused;if(p){allocated--;free(p);}}
static ISzAlloc alloc={trackedAlloc,trackedFree};
static void check(int ok,const char *name){if(!ok){fprintf(stdout,"FAIL %s: %s (allocations %u)\n",name,report.message,allocated);exit(2);}printf("PASS %s\n",name);passed++;}
static void path(char *out,const char *name){size_t n=strlen(work);strcpy(out,work);out[n++]=Z7_PATH_SEPARATOR;strcpy(out+n,name);}
static void dataFile(const char *name,unsigned long size){FILE *file=fopen(name,"wb");unsigned long i;check(file!=NULL,"create private fixture");for(i=0;i<size;i++)if(fputc((int)((i*31+7)&255),file)==EOF)exit(3);if(fclose(file))exit(3);}
static int exists(const char *name){struct stat info;return !fsStat(name,&info);}
static int equalFiles(const char *a,const char *b){FILE *left=fopen(a,"rb"),*right=fopen(b,"rb");int x,y,ok=1;if(!left||!right){if(left)fclose(left);if(right)fclose(right);return 0;}do{x=fgetc(left);y=fgetc(right);if(x!=y){ok=0;break;}}while(x!=EOF);if(ferror(left)||ferror(right))ok=0;if(fclose(left)||fclose(right))ok=0;return ok;}
static void noStaging(void){
#ifdef __WATCOMC__
 struct find_t item;char pattern[260];int ok;path(pattern,"7F*.CPY");ok=_dos_findfirst(pattern,_A_NORMAL|_A_HIDDEN|_A_SYSTEM|_A_RDONLY|_A_SUBDIR,&item)!=0;path(pattern,"7F*.BAK");ok&=_dos_findfirst(pattern,_A_NORMAL|_A_HIDDEN|_A_SYSTEM|_A_RDONLY|_A_SUBDIR,&item)!=0;check(ok,"no staging or backup files remain");
#else
 DIR *dir=opendir(work);struct dirent *item;int ok=1;check(dir!=NULL,"scan fixture directory");while((item=readdir(dir))!=NULL)if(!strncmp(item->d_name,"7F",2)&&(strstr(item->d_name,".CPY")||strstr(item->d_name,".BAK")))ok=0;closedir(dir);check(ok,"no staging or backup files remain");
#endif
}
static int progressCallback(UInt64 bytes){
 if(cancelBackup&&lastBackup[0]&&exists(lastBackup))return 0;
 if(changeDestination&&bytes>=65539UL){
  FILE *file;struct stat info;struct utimbuf times;
  if(stat(dst,&info))return 0;times.actime=info.st_atime;times.modtime=info.st_mtime;
  file=fopen(dst,"r+b");if(!file)return 0;if(fputc(99,file)==EOF){fclose(file);return 0;}
  if(fclose(file)||utime(dst,&times))return 0;changeDestination=0;
 }
 if(collisionEnabled&&bytes>=65539UL&&!exists(dst)){FILE *f=fopen(dst,"wb");if(!f)return 0;fputs("USER DATA",f);fclose(f);collisionEnabled=0;}
 if(mutationEnabled&&bytes>=65539UL){FILE *f=fopen(src,"r+b");if(!f)return 0;fputc(99,f);fclose(f);mutationEnabled=0;}
 return !(cancelEnabled&&bytes>=cancelAt);
}
static SRes run(unsigned op,const char *source,const char *dest){SRes result;allocationCalls=0;lastBackup[0]=0;result=Z7Win16_FileOperation(op,source,dest,&alloc,progressCallback,&report);check(!allocated,"operation releases every tracked allocation");return result;}
static void clean(const char *name){check(run(Z7_FS_DELETE,name,NULL)==SZ_OK,"remove owned test output");}
static void moveReplacementTests(void){
 SRes result;unsigned total,i;
 path(extra,"NEWREF.BIN");dataFile(extra,65539UL);path(nested,"OLDREF.BIN");dataFile(nested,17);dataFile(dst,17);
 check(run(Z7_FS_MOVE_REPLACE,src,dst)==SZ_OK&&!exists(src)&&equalFiles(dst,extra),"confirmed move replaces destination and removes source after commit");total=allocationCalls;
 check(run(Z7_FS_RENAME,dst,src)==SZ_OK,"restore move replacement source");dataFile(dst,17);
 for(i=1;i<=total;i++){failAllocation=i;result=run(Z7_FS_MOVE_REPLACE,src,dst);failAllocation=0;check(result==SZ_ERROR_MEM&&equalFiles(src,extra)&&equalFiles(dst,nested),"move replacement allocation failure preserves both files");noStaging();}
 for(i=0;i<3;i++){cancelEnabled=1;cancelAt=i==0?0:i==1?4096:65539;result=run(Z7_FS_MOVE_REPLACE,src,dst);cancelEnabled=0;check(result==SZ_ERROR_PROGRESS&&equalFiles(src,extra)&&equalFiles(dst,nested),"cancelled move replacement preserves source and original destination");noStaging();}
 failWrite=1;writeCalls=0;result=run(Z7_FS_MOVE_REPLACE,src,dst);failWrite=0;check(result==SZ_ERROR_WRITE&&equalFiles(src,extra)&&equalFiles(dst,nested),"move replacement write failure preserves both files");noStaging();
 failCommit=1;result=run(Z7_FS_MOVE_REPLACE,src,dst);failCommit=0;check(result==SZ_ERROR_WRITE&&equalFiles(src,extra)&&equalFiles(dst,nested),"failed move publication restores destination and keeps source");noStaging();
 cancelBackup=1;result=run(Z7_FS_MOVE_REPLACE,src,dst);cancelBackup=0;check(result==SZ_ERROR_PROGRESS&&equalFiles(src,extra)&&equalFiles(dst,nested),"cancelled move after backup restores original destination");noStaging();
 failBackupRemove=1;result=run(Z7_FS_MOVE_REPLACE,src,dst);failBackupRemove=0;
 check(result==SZ_ERROR_WRITE&&equalFiles(src,extra)&&equalFiles(dst,extra)&&equalFiles(lastBackup,nested)&&strstr(report.message,"source kept"),"failed old-file cleanup keeps source, complete new copy and old backup");check(!remove(lastBackup),"remove retained move backup");clean(dst);dataFile(dst,17);noStaging();
 strcpy(crossSource,src);mutateOnPublish=1;result=run(Z7_FS_MOVE_REPLACE,src,dst);mutateOnPublish=0;
 check(result==SZ_ERROR_READ&&exists(src)&&!equalFiles(src,extra)&&equalFiles(dst,extra)&&strstr(report.message,"source verification failed"),"same-size source edit after publication is never deleted");clean(src);check(run(Z7_FS_COPY,extra,src)==SZ_OK,"restore edited move source");clean(dst);dataFile(dst,17);noStaging();
 failSourceRemove=1;result=run(Z7_FS_MOVE_REPLACE,src,dst);failSourceRemove=0;check(result==SZ_ERROR_WRITE&&equalFiles(src,extra)&&equalFiles(dst,extra)&&strstr(report.message,"could not be removed"),"source removal failure reports both complete copies");clean(dst);noStaging();
 check(run(Z7_FS_MOVE_REPLACE,src,src)==SZ_ERROR_PARAM&&equalFiles(src,extra),"self move replacement cannot remove its source");
 clean(extra);clean(nested);
}
int main(int argc,char **argv){SRes result;unsigned total,i;struct stat before,after;struct utimbuf times;
#ifdef __WATCOMC__
 if(argc!=2||strlen(argv[1])>=sizeof(work))return 7;strcpy(work,argv[1]);if(exists(work)||Z7_MKDIR(work))return 7;
#else
 (void)argc;(void)argv;strcpy(work,"/tmp/F6XXXXXX");if(!mkdtemp(work))return 7;
#endif
 CrcGenerateTable();path(src,"SRC.BIN");path(dst,"COPY.BIN");dataFile(src,65539UL);times.actime=times.modtime=946684802UL;check(!utime(src,&times),"set representable source timestamp");
 check(run(Z7_FS_COPY,src,dst)==SZ_OK&&equalFiles(src,dst),"copy crosses 16-bit byte-count boundary with exact content");check(!stat(src,&before)&&!stat(dst,&after)&&before.st_mtime==after.st_mtime,"copy preserves file modification time");clean(dst);
 for(i=1;i<=3;i++){cancelEnabled=1;cancelAt=i==1?0:i==2?4096:65539;result=run(Z7_FS_COPY,src,dst);cancelEnabled=0;check(result==SZ_ERROR_PROGRESS&&!exists(dst)&&exists(src),"cancelled copy never publishes a destination");noStaging();}
 failWrite=1;writeCalls=0;result=run(Z7_FS_COPY,src,dst);failWrite=0;check(result==SZ_ERROR_WRITE&&!exists(dst),"write failure rolls back staged file");noStaging();
 failRead=2;readCalls=0;result=run(Z7_FS_COPY,src,dst);failRead=0;check(result==SZ_ERROR_READ&&!exists(dst),"short source input rejects incomplete copy");noStaging();
 for(i=1;i<=2;i++){failClose=i;closeCalls=0;result=run(Z7_FS_COPY,src,dst);failClose=0;check(result!=SZ_OK&&!exists(dst),"source or destination close failure rolls back");noStaging();}
 failCommit=1;result=run(Z7_FS_COPY,src,dst);failCommit=0;check(result==SZ_ERROR_WRITE&&!exists(dst)&&exists(src),"commit failure preserves source and removes staging");noStaging();
 dataFile(dst,17);result=run(Z7_FS_COPY,src,dst);check(result!=SZ_OK&&!stat(dst,&after)&&after.st_size==17,"existing destination is never overwritten");clean(dst);
 collisionEnabled=1;result=run(Z7_FS_COPY,src,dst);collisionEnabled=0;check(result!=SZ_OK&&!stat(dst,&after)&&after.st_size==9,"destination appearing during copy remains untouched");clean(dst);noStaging();
 path(extra,"OLD.BIN");dataFile(extra,17);dataFile(dst,17);
 check(run(Z7_FS_COPY_REPLACE,src,dst)==SZ_OK&&equalFiles(src,dst),"explicit replacement publishes exact new contents");total=allocationCalls;clean(dst);
 dataFile(dst,17);
 for(i=1;i<=total;i++){failAllocation=i;result=run(Z7_FS_COPY_REPLACE,src,dst);failAllocation=0;check(result==SZ_ERROR_MEM&&equalFiles(dst,extra),"replacement allocation failure preserves old destination");noStaging();}
 for(i=0;i<3;i++){cancelEnabled=1;cancelAt=i==0?0:i==1?4096:65539;result=run(Z7_FS_COPY_REPLACE,src,dst);cancelEnabled=0;check(result==SZ_ERROR_PROGRESS&&equalFiles(dst,extra),"cancelled replacement preserves old destination");noStaging();}
 failWrite=1;writeCalls=0;result=run(Z7_FS_COPY_REPLACE,src,dst);failWrite=0;check(result==SZ_ERROR_WRITE&&equalFiles(dst,extra),"replacement write failure preserves old destination");noStaging();
 failRead=1;readCalls=0;result=run(Z7_FS_COPY_REPLACE,src,dst);failRead=0;check(result==SZ_ERROR_READ&&equalFiles(dst,extra),"destination snapshot read failure aborts replacement");noStaging();
 failClose=1;closeCalls=0;result=run(Z7_FS_COPY_REPLACE,src,dst);failClose=0;check(result==SZ_ERROR_READ&&equalFiles(dst,extra),"destination snapshot close failure aborts replacement");noStaging();
 failCommit=1;result=run(Z7_FS_COPY_REPLACE,src,dst);failCommit=0;check(result==SZ_ERROR_WRITE&&equalFiles(dst,extra)&&lastBackup[0]&&!exists(lastBackup),"failed replacement commit restores original destination");noStaging();
 cancelBackup=1;result=run(Z7_FS_COPY_REPLACE,src,dst);cancelBackup=0;check(result==SZ_ERROR_PROGRESS&&equalFiles(dst,extra)&&lastBackup[0]&&!exists(lastBackup),"cancellation after backup restores original destination");noStaging();
 changeDestination=1;result=run(Z7_FS_COPY_REPLACE,src,dst);changeDestination=0;check(result==SZ_ERROR_WRITE&&!equalFiles(dst,extra)&&!stat(dst,&after)&&after.st_size==17,"same-size destination edit with unchanged time is preserved");clean(dst);noStaging();dataFile(dst,17);
#ifdef __WATCOMC__
 check(!_dos_setfileattr(dst,_A_RDONLY|_A_HIDDEN|_A_SYSTEM),"set protected replacement destination attributes");
#else
 check(!chmod(dst,0444),"set readonly replacement destination");
#endif
 failCommit=1;result=run(Z7_FS_COPY_REPLACE,src,dst);failCommit=0;check(result==SZ_ERROR_WRITE&&equalFiles(dst,extra)&&!stat(dst,&after)&&!(after.st_mode&S_IWUSR),"failed replacement retains readonly target and its metadata");noStaging();
 check(run(Z7_FS_COPY_REPLACE,src,dst)==SZ_OK&&equalFiles(src,dst),"confirmed replacement handles readonly target");clean(dst);dataFile(dst,17);
 failCommit=failRestore=1;result=run(Z7_FS_COPY_REPLACE,src,dst);failCommit=failRestore=0;
 check(result==SZ_ERROR_WRITE&&!exists(dst)&&lastBackup[0]&&equalFiles(lastBackup,extra)&&strstr(report.message,lastBackup),"failed rollback retains original bytes at reported recovery path");check(!rename(lastBackup,dst),"restore retained test backup manually");noStaging();
 failCommit=failRestore=failStagingRemove=1;result=run(Z7_FS_COPY_REPLACE,src,dst);failCommit=failRestore=failStagingRemove=0;path(nested,"7F000000.CPY");
 check(result==SZ_ERROR_WRITE&&!exists(dst)&&equalFiles(lastBackup,extra)&&equalFiles(nested,src)&&strstr(report.message,fsLeaf(lastBackup))&&strstr(report.message,fsLeaf(nested))&&strstr(report.message,work),"combined rollback and cleanup failures report both recoverable files");check(!rename(lastBackup,dst)&&!remove(nested),"recover both retained test files");noStaging();
 failBackupRemove=1;result=run(Z7_FS_COPY_REPLACE,src,dst);failBackupRemove=0;
 check(result==SZ_ERROR_WRITE&&equalFiles(src,dst)&&lastBackup[0]&&equalFiles(lastBackup,extra)&&strstr(report.message,lastBackup),"backup cleanup failure reports both complete new copy and retained old bytes");check(!remove(lastBackup),"remove retained test backup");clean(dst);noStaging();
 path(nested,"7F000000.BAK");dataFile(nested,17);dataFile(dst,17);
 check(run(Z7_FS_COPY_REPLACE,src,dst)==SZ_OK&&equalFiles(src,dst)&&equalFiles(nested,extra),"pre-existing backup-shaped user file is never replaced");clean(dst);clean(nested);
 path(dst,"7F000000.BAK");dataFile(dst,17);check(run(Z7_FS_COPY_REPLACE,src,dst)==SZ_OK&&equalFiles(src,dst),"backup-shaped destination never becomes its own backup");clean(dst);clean(extra);path(dst,"COPY.BIN");noStaging();
 moveReplacementTests();
 path(extra,"MOVED.BIN");check(run(Z7_FS_MOVE,src,extra)==SZ_OK&&!exists(src)&&exists(extra),"same-volume move renames without copying");check(run(Z7_FS_RENAME,extra,src)==SZ_OK&&exists(src)&&!exists(extra),"rename preserves file and removes only old name");
 strcpy(crossSource,src);forceCrossMove=1;check(run(Z7_FS_MOVE,src,dst)==SZ_OK&&!exists(src)&&exists(dst),"cross-volume file move copies then removes source");forceCrossMove=0;check(run(Z7_FS_RENAME,dst,src)==SZ_OK,"restore private move fixture");
 strcpy(crossSource,src);forceCrossMove=1;mutationEnabled=1;result=run(Z7_FS_MOVE,src,dst);forceCrossMove=mutationEnabled=0;check(result!=SZ_OK&&exists(src)&&!exists(dst),"source mutation attempt cannot commit an unsafe cross-volume move");noStaging();
 path(extra,"TREE");check(run(Z7_FS_MKDIR,NULL,extra)==SZ_OK,"create folder");path(nested,"TREE");strcat(nested,"/SUB");
#ifdef __WATCOMC__
 {char *p;for(p=nested;*p;p++)if(*p=='/')*p='\\';}
#endif
 check(run(Z7_FS_MKDIR,NULL,nested)==SZ_OK,"create nested folder");strcat(nested,Z7_PATH_SEPARATOR=='/'?"/EMPTY":"\\EMPTY");check(run(Z7_FS_MKDIR,NULL,nested)==SZ_OK,"create empty descendant");
 #ifdef __WATCOMC__
 path(nested,"TREE");strcat(nested,"\\SUB");check(!_dos_setfileattr(nested,_A_RDONLY|_A_HIDDEN|_A_SYSTEM),"set hidden system readonly directory");
#endif
 path(nested,"TREE");strcat(nested,Z7_PATH_SEPARATOR=='/'?"/A.BIN":"\\A.BIN");dataFile(nested,65539UL);
 path(nested,"TREE");strcat(nested,Z7_PATH_SEPARATOR=='/'?"/HIDDEN.BIN":"\\HIDDEN.BIN");dataFile(nested,11);
#ifdef __WATCOMC__
 check(!_dos_setfileattr(nested,_A_RDONLY|_A_HIDDEN|_A_SYSTEM),"set hidden system readonly source");
#else
 check(!chmod(nested,0444),"set readonly source");
#endif
 path(dst,"ROCOPY.BIN");check(run(Z7_FS_COPY,nested,dst)==SZ_OK&&equalFiles(nested,dst),"copy standalone readonly file");clean(dst);
 path(dst,"TREECOPY");check(run(Z7_FS_COPY,extra,dst)==SZ_OK,"copy complete directory tree");total=allocationCalls;
 path(nested,"TREECOPY");strcat(nested,Z7_PATH_SEPARATOR=='/'?"/SUB/EMPTY":"\\SUB\\EMPTY");check(exists(nested),"copy preserves empty descendant folder");
 path(nested,"TREECOPY");strcat(nested,Z7_PATH_SEPARATOR=='/'?"/HIDDEN.BIN":"\\HIDDEN.BIN");check(!stat(nested,&after)&&!(after.st_mode&S_IWUSR),"tree copy preserves readonly attribute");clean(dst);
 for(i=1;i<=total;i++){failAllocation=i;result=run(Z7_FS_COPY,extra,dst);failAllocation=0;sprintf(detail,"allocation failure %u/%u leaves source intact and destination absent",i,total);check(result==SZ_ERROR_MEM&&exists(extra)&&!exists(dst),detail);noStaging();}
 path(dst,"TREE");strcat(dst,Z7_PATH_SEPARATOR=='/'?"/INSIDE":"\\INSIDE");check(run(Z7_FS_COPY,extra,dst)!=SZ_OK&&!exists(dst),"reject copying a directory into itself");
 path(dst,"RENAMED");check(run(Z7_FS_RENAME,extra,dst)==SZ_OK&&!exists(extra),"rename directory with descendants");cancelEnabled=1;cancelAt=1;result=run(Z7_FS_DELETE,dst,NULL);cancelEnabled=0;check(result==SZ_ERROR_PROGRESS&&exists(dst)&&report.files+report.folders==1,"cancelled deletion reports partial work accurately");clean(dst);
 clean(src);noStaging();check(run(Z7_FS_DELETE,work,NULL)==SZ_OK,"remove private fixture root");printf("PASS ALL FILESYSTEM TESTS (%u checks)\n",passed);return 0;
}
