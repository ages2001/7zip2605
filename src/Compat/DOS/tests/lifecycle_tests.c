/* Real files plus deterministic I/O, cancellation and recovery failures.
   Run only in a newly created private directory. */
#ifndef __WATCOMC__
#define _GNU_SOURCE
#define Z7_DOS_HOST_TEST
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef __WATCOMC__
#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#endif
static unsigned failRename,renameCalls,failWrite,writeCalls,failRead,readCalls,failClose,closeCalls;
static unsigned cancelRename,cancelWrite;
static int failRestore,failCleanup;
static void testCancel(void);
static int probeRename(const char *source,const char *destination){
 int result;
 ++renameCalls;
 if((failRename&&renameCalls==failRename)||(failRestore&&strstr(source,"OLD"))){errno=EACCES;return -1;}
#ifdef __WATCOMC__
 result=rename(source,destination);
#else
 result=(int)syscall(SYS_renameat2,AT_FDCWD,source,AT_FDCWD,destination,1);
#endif
 if(!result&&cancelRename==renameCalls)testCancel();return result;
}
static size_t probeWrite(const void *data,size_t size,size_t count,FILE *file){
 size_t result;++writeCalls;
 if(failWrite==writeCalls)return 0;
 result=fwrite(data,size,count,file);if(cancelWrite==writeCalls)testCancel();return result;
}
static size_t probeRead(void *data,size_t size,size_t count,FILE *file){++readCalls;if(failRead==readCalls)return 0;return fread(data,size,count,file);}
static int probeClose(FILE *file){int result=fclose(file);++closeCalls;return failClose==closeCalls?EOF:result;}
static int probeRemove(const char *path){if(failCleanup&&strstr(path,"OLD")){errno=EACCES;return -1;}return remove(path);}
#define rename probeRename
#define fwrite probeWrite
#define fread probeRead
#define fclose probeClose
#define remove probeRemove
#define main unusedInstallerMain
#include "../lifecycle.c"
#undef main
#undef rename
#undef fwrite
#undef fread
#undef fclose
#undef remove
static unsigned checks;static char work[260],destination[260],payloadPath[260];
static Byte initial[2*RECORD_SIZE];
static const char oldA[]="old executable bytes\r\n",oldB[]="old document\r\n",newA[]="new executable bytes\r\n",newC[]="new license\r\n",user[]="user-owned bytes\r\n";
static void testCancel(void){interrupted=1;}
static void check(int ok,const char *message){if(!ok){printf("FAIL %s (%d)\n",message,errno);exit(2);}++checks;}
static void resetFaults(void){
 failRename=renameCalls=failWrite=writeCalls=failRead=readCalls=failClose=closeCalls=0;
 cancelRename=cancelWrite=0;failRestore=failCleanup=0;interrupted=0;
}
static void writeBytes(const char *path,const void *data,size_t size){FILE *file=fopen(path,"wb");check(file!=NULL,"create fixture");check(fwrite(data,1,size,file)==size,"write fixture");check(!fclose(file),"close fixture");}
static void namedBytes(const char *directory,const char *name,const char *data){char path[260];lifePath(path,directory,name);writeBytes(path,data,strlen(data));}
static int same(const char *directory,const char *name,const char *data){
 char path[260],bytes[256];FILE *file;size_t n;int ok;
 lifePath(path,directory,name);file=fopen(path,"rb");if(!file)return 0;
 n=fread(bytes,1,sizeof(bytes),file);ok=n==strlen(data)&&!memcmp(bytes,data,n)&&!ferror(file);if(fclose(file))ok=0;return ok;
}
static int present(const char *directory,const char *name){char path[260];lifePath(path,directory,name);return lifeExists(path);}
static void record(Byte *r,const char *name,const char *data,UInt32 offset){
 memset(r,0,RECORD_SIZE);strcpy((char *)r,name);lifePut32(r+16,offset);lifePut32(r+20,(UInt32)strlen(data));lifePut32(r+24,CrcCalc(data,strlen(data)));
}
static void payload(void){
 Byte mz[512],footer[24];FILE *file;
 memset(mz,0,sizeof(mz));mz[0]='M';mz[1]='Z';mz[4]=1;
 record(table,"A.EXE",newA,512);record(table+RECORD_SIZE,"C.TXT",newC,512+strlen(newA));
 memcpy(footer,"7ZDS31\0\0",8);lifePut32(footer+8,512+strlen(newA)+strlen(newC));lifePut32(footer+12,2);lifePut32(footer+16,2*RECORD_SIZE);lifePut32(footer+20,CrcCalc(table,2*RECORD_SIZE));
 file=fopen(payloadPath,"wb");check(file!=NULL,"open payload fixture");
 check(fwrite(mz,1,sizeof(mz),file)==sizeof(mz)&&fwrite(newA,1,strlen(newA),file)==strlen(newA)&&fwrite(newC,1,strlen(newC),file)==strlen(newC)&&fwrite(table,1,2*RECORD_SIZE,file)==2*RECORD_SIZE&&fwrite(footer,1,24,file)==24,"write checked payload");check(!fclose(file),"close payload");
}
static void seed(void){
 resetFaults();check(!lifeMkdir(destination),"create old installation");
 record(initial,"A.EXE",oldA,0);record(initial+RECORD_SIZE,"B.TXT",oldB,0);
 namedBytes(destination,"A.EXE",oldA);namedBytes(destination,"B.TXT",oldB);namedBytes(destination,"KEEP.TXT",user);
 check(lifeManifestWrite(destination,initial,2),"write old ownership manifest");
}
static void noStage(void){unsigned i;char path[260],name[13];
 for(i=0;i<10;i++){sprintf(name,"Z7S%04u.TMP",i);lifePath(path,work,name);check(!lifeExists(path),"no temporary directory remains");}
}
static void oldIntact(void){
 Byte records[2*RECORD_SIZE];unsigned count;
 check(same(destination,"A.EXE",oldA)&&same(destination,"B.TXT",oldB),"original payloads unchanged");
 check(same(destination,"KEEP.TXT",user)&&!present(destination,"C.TXT"),"unrelated file preserved and new file absent");
 check(lifeManifestRead(destination,records,&count)&&count==2&&!memcmp(records,initial,sizeof(initial)),"original manifest restored");noStage();
}
static void newIntact(void){
 Byte records[2*RECORD_SIZE];unsigned count;
 check(same(destination,"A.EXE",newA)&&same(destination,"C.TXT",newC)&&!present(destination,"B.TXT"),"new payloads published and obsolete file removed");
 check(same(destination,"KEEP.TXT",user),"unrelated file retained on upgrade");
 check(lifeManifestRead(destination,records,&count)&&count==2&&!memcmp(records,table,2*RECORD_SIZE),"new ownership manifest installed");
}
static void clear(void){
 static const char *names[]={"A.EXE","B.TXT","C.TXT","KEEP.TXT",LIFE_MANIFEST};unsigned i;char path[260];
 resetFaults();for(i=0;i<sizeof(names)/sizeof(names[0]);i++){lifePath(path,destination,names[i]);check(lifeRemove(path),"remove private fixture");}
 check(!rmdir(destination),"remove private installation directory");noStage();
}
static int install(int upgrade){FILE *file=fopen(payloadPath,"rb");unsigned count;int result;
 check(file!=NULL&&lifePayload(file,&count)&&count==2,"validate independent payload");
 result=lifeInstall(file,destination,count,upgrade);check(!fclose(file),"close test payload");return result;
}
static void faultMatrix(unsigned kind){
 unsigned point;int result;FILE *file;unsigned count;
 for(point=1;point<150;point++){
  seed();file=fopen(payloadPath,"rb");check(file!=NULL&&lifePayload(file,&count),"open payload before fault injection");resetFaults();
  switch(kind){case 0:failRename=point;break;case 1:failWrite=point;break;case 2:failRead=point;break;case 3:failClose=point;break;case 4:cancelRename=point;break;case 5:cancelWrite=point;break;}
  result=lifeInstall(file,destination,count,1);fclose(file);resetFaults();
  if(result==0||result==1){newIntact();noStage();clear();break;}
  check(result==2||result==255,"operation reports fault or cancellation");oldIntact();clear();
 }
 check(point<150,"finite fault matrix");printf("PASS upgrade fault kind %u: %u injection points\n",kind,point-1);
}
int main(int argc,char **argv){
 unsigned i;int result;char path[260],source[260],target[260];Byte saved[2*RECORD_SIZE];
 if(argc!=2||strlen(argv[1])>180)return 7;strcpy(work,argv[1]);lifePath(destination,work,"APP");lifePath(payloadPath,work,"DATA.BIN");
 check(!lifeMkdir(work),"reserve new private test directory");CrcGenerateTable();payload();
 check(!install(0),"fresh install");check(same(destination,"A.EXE",newA)&&same(destination,"C.TXT",newC),"fresh payload bytes");noStage();clear();
 seed();check(install(0)==2,"existing folder requires explicit upgrade");oldIntact();clear();
 seed();check(!install(1),"upgrade");newIntact();noStage();clear();
 seed();lifePath(path,destination,"A.EXE");check(!remove(path),"simulate missing installed file");check(!install(1),"upgrade repairs missing owned file");newIntact();noStage();clear();
 seed();namedBytes(destination,"A.EXE",user);check(install(1)==2,"modified installed file blocks upgrade");check(same(destination,"A.EXE",user)&&same(destination,"B.TXT",oldB),"modified file preserved");noStage();clear();
 seed();namedBytes(destination,"C.TXT",user);check(install(1)==2,"unrelated name collision blocks upgrade");check(same(destination,"C.TXT",user)&&same(destination,"A.EXE",oldA),"collision bytes preserved");noStage();clear();
 seed();lifePath(path,destination,LIFE_MANIFEST);writeBytes(path,"bad",3);check(install(1)==2&&lifeUninstall(destination)==2,"damaged ownership blocks upgrade and removal");check(same(destination,"A.EXE",oldA),"unrecorded file retained");noStage();clear();
 for(i=0;i<6;i++)faultMatrix(i);
 seed();resetFaults();failRename=5;failRestore=1;result=install(1);resetFaults();check(result==2,"restoration failure reported");
 check(same(backup,"A.EXE",oldA)&&same(backup,"B.TXT",oldB)&&present(backup,LIFE_MANIFEST),"failed restoration retains complete backups and manifest");
 for(i=0;i<2;i++){lifePath(source,backup,(char *)(initial+i*RECORD_SIZE));lifePath(target,destination,(char *)(initial+i*RECORD_SIZE));check(!rename(source,target),"recover original file");}
 lifePath(source,backup,LIFE_MANIFEST);lifePath(target,destination,LIFE_MANIFEST);check(!rename(source,target),"recover original manifest");check(lifeDiscard(2,2),"clean recovered transaction");oldIntact();clear();
 seed();failCleanup=1;check(install(1)==1,"backup cleanup warning after committed upgrade");resetFaults();newIntact();check(same(backup,"A.EXE",oldA),"cleanup failure retains old copy");check(lifeDiscard(2,2),"finish deferred cleanup");clear();
 for(i=1;i<=4;i++){
  seed();resetFaults();failRename=i;result=lifeUninstall(destination);resetFaults();
  if(i<=3){check(result==2,"uninstall rename failure");oldIntact();}
  else{check(!result,"uninstall succeeds after final rename");check(!present(destination,"A.EXE")&&!present(destination,"B.TXT")&&!present(destination,LIFE_MANIFEST)&&same(destination,"KEEP.TXT",user),"uninstall preserves unrelated file");}
  clear();
 }
 seed();namedBytes(destination,"A.EXE",user);check(!lifeUninstall(destination),"uninstall modified file");check(same(destination,"A.EXE",user)&&same(destination,"KEEP.TXT",user)&&!present(destination,"B.TXT"),"uninstall preserves modified and unrelated files");clear();
 seed();lifePath(path,destination,"A.EXE");
#ifdef __WATCOMC__
 check(!lifeSetAttributes(path,_A_RDONLY|_A_HIDDEN|_A_SYSTEM),"set owned DOS attributes");
#else
 check(!lifeSetAttributes(path,0400),"set owned readonly attribute");
#endif
 check(!lifeUninstall(destination),"remove owned readonly file");check(!present(destination,"A.EXE")&&same(destination,"KEEP.TXT",user),"readonly removal preserves unrelated file");clear();
 for(i=1;i<=3;i++){
  seed();resetFaults();cancelRename=i;result=lifeUninstall(destination);resetFaults();
  if(i<3){check(result==255,"uninstall cancellation before commit");oldIntact();}
  else check(!result&&!present(destination,LIFE_MANIFEST)&&same(destination,"KEEP.TXT",user),"cancellation after commit keeps completed removal");
  clear();
 }
 seed();lifePath(path,destination,"KEEP.TXT");check(!remove(path),"remove unrelated fixture");check(!lifeUninstall(destination)&&!lifeExists(destination),"uninstall removes empty installation directory");noStage();
 /* Manifest names cannot escape the installation or impersonate its record. */
 memcpy(saved,table,sizeof(saved));strcpy((char *)table,"..\\BAD.EXE");check(!lifeRecordsValid(table,2),"reject traversal manifest");memcpy(table,saved,sizeof(saved));strcpy((char *)table,LIFE_MANIFEST);check(!lifeRecordsValid(table,2),"reject self-referential manifest");memcpy(table,saved,sizeof(saved));memcpy(table+RECORD_SIZE,table,RECORD_SIZE);check(!lifeRecordsValid(table,2),"reject duplicate manifest");
 check(!remove(payloadPath)&&!rmdir(work),"remove private test fixtures");printf("PASS DOS installer lifecycle: %u checks\n",checks);return 0;
}
