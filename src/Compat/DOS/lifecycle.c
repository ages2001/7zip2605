/* Native DOS installation, explicit upgrades and ownership-based removal.
   SPDX-License-Identifier: LGPL-2.1-or-later
   Payload format is shared with Win16; its installer behavior is unchanged. */
#define Z7_SETUP_HELPERS_ONLY
#include "setup.c"
#ifdef Z7_DOS_HOST_TEST
#include <dirent.h>
#include <strings.h>
#define stricmp strcasecmp
#define O_BINARY 0
#define LIFE_SEP '/'
static int lifeMkdir(const char *path){return mkdir(path,0700);}
static int lifeAttributes(const char *path,unsigned *attributes){
 struct stat info;if(stat(path,&info))return -1;*attributes=(unsigned)info.st_mode;return 0;
}
static int lifeSetAttributes(const char *path,unsigned attributes){return chmod(path,attributes);}
#define LIFE_WRITABLE 0600
#else
#include <dos.h>
#define LIFE_SEP '\\'
#define lifeMkdir mkdir
#define lifeAttributes _dos_getfileattr
#define lifeSetAttributes _dos_setfileattr
#define LIFE_WRITABLE 0
#endif
#define LIFE_MANIFEST "INSTALL.DAT"
/* Name of the primary executable to run after install, shown in the completion
   message. Defaults to the 32-bit DOS build; the dos16 build overrides it. */
#ifndef Z7_DOS_MAINEXE
#define Z7_DOS_MAINEXE "7Z.EXE"
#endif
/* Installer executable name shown in the usage help; set per build. */
#ifndef Z7_DOS_SETUPNAME
#define Z7_DOS_SETUPNAME "7ZSETDOS"
#endif
static const char lifeSignature[8]={'7','Z','D','S','7','7',0,0};
static Byte oldTable[MAX_FILES*RECORD_SIZE];
static char stage[260],backup[260];
static void lifePut32(Byte *p,UInt32 value){unsigned i;for(i=0;i<4;i++){p[i]=(Byte)value;value>>=8;}}
static void lifePath(char *out,const char *directory,const char *name){sprintf(out,"%s%c%s",directory,LIFE_SEP,name);}
static int lifeExists(const char *path){struct stat info;if(!stat(path,&info))return 1;return errno==ENOENT?0:-1;}
static int lifeRecordsValid(const Byte *records,unsigned count){
 unsigned i,j,k;
 if(!count||count>MAX_FILES)return 0;
 for(i=0;i<count;i++){
  const Byte *record=records+i*RECORD_SIZE;const char *name=(const char *)record;
  if(!memchr(name,0,13)||!validName(name)||!stricmp(name,LIFE_MANIFEST))return 0;
  for(k=(unsigned)strlen(name);k<16;k++)if(record[k])return 0;
  for(j=0;j<i;j++)if(!stricmp(name,(const char *)(records+j*RECORD_SIZE)))return 0;
  if(get32(record+20)>0x7fffffffUL)return 0;
 }
 return 1;
}
static int lifeManifestRead(const char *directory,Byte *records,unsigned *count){
 char path[260];Byte header[20];FILE *file;unsigned size;int ok=0;
 lifePath(path,directory,LIFE_MANIFEST);file=fopen(path,"rb");if(!file)return 0;
 if(fread(header,1,20,file)!=20||memcmp(header,lifeSignature,8)||!get32(header+8)||get32(header+8)>MAX_FILES)goto done;
 *count=(unsigned)get32(header+8);size=*count*RECORD_SIZE;
 if(get32(header+12)!=size||fread(records,1,size,file)!=size||fgetc(file)!=EOF||ferror(file))goto done;
 ok=CrcCalc(records,size)==get32(header+16)&&lifeRecordsValid(records,*count);
 done:if(fclose(file))ok=0;return ok;
}
static int lifeManifestWrite(const char *directory,const Byte *records,unsigned count){
 char path[260];Byte header[20];FILE *file;int fd,ok;
 lifePath(path,directory,LIFE_MANIFEST);
 fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);if(fd<0)return 0;
 file=fdopen(fd,"wb");if(!file){close(fd);remove(path);return 0;}
 memcpy(header,lifeSignature,8);lifePut32(header+8,count);lifePut32(header+12,count*RECORD_SIZE);
 lifePut32(header+16,CrcCalc(records,count*RECORD_SIZE));
 ok=fwrite(header,1,20,file)==20&&fwrite(records,RECORD_SIZE,count,file)==count;
 if(fclose(file))ok=0;if(!ok)remove(path);return ok;
}
static int lifeMatches(const char *path,const Byte *record){
 FILE *file=fopen(path,"rb");UInt32 left=get32(record+20),crc=CRC_INIT_VAL;int ok;
 if(!file)return 0;
 while(left){size_t n=left>sizeof(buffer)?sizeof(buffer):(size_t)left;
  if(interrupted||fread(buffer,1,n,file)!=n){fclose(file);return 0;}
  crc=CrcUpdate(crc,buffer,n);left-=n;
 }
 ok=fgetc(file)==EOF&&!ferror(file)&&CRC_GET_DIGEST(crc)==get32(record+24);
 if(fclose(file))ok=0;return ok;
}
static int lifeFind(const Byte *records,unsigned count,const char *name){
 unsigned i;for(i=0;i<count;i++)if(!stricmp(name,(const char *)(records+i*RECORD_SIZE)))return (int)i;return -1;
}
static int lifePayload(FILE *self,unsigned *count){
 Byte footer[24],mz[6];UInt32 start,size,previous=0,minimum;long length;unsigned i;
 if(fseek(self,0,SEEK_END)||(length=ftell(self))<24||fseek(self,length-24,SEEK_SET)||fread(footer,1,24,self)!=24)return 0;
 if(memcmp(footer,"7ZDS31\0\0",8)||!get32(footer+12)||get32(footer+12)>MAX_FILES)return 0;
 *count=(unsigned)get32(footer+12);start=get32(footer+8);size=get32(footer+16);
 if(size!=*count*RECORD_SIZE||start>(UInt32)length-24||size!=(UInt32)length-24-start)return 0;
 if(fseek(self,0,SEEK_SET)||fread(mz,1,6,self)!=6||memcmp(mz,"MZ",2))return 0;
 minimum=(UInt32)mz[4]+((UInt32)mz[5]<<8);
 if(!minimum||((unsigned)mz[2]+((unsigned)mz[3]<<8))>511)return 0;
 minimum=(minimum-1)*512+((mz[2]||mz[3])?((unsigned)mz[2]+((unsigned)mz[3]<<8)):512);
 if(fseek(self,(long)start,SEEK_SET)||fread(table,1,(size_t)size,self)!=(size_t)size||CrcCalc(table,(size_t)size)!=get32(footer+20)||!lifeRecordsValid(table,*count))return 0;
 for(i=0;i<*count;i++){
  const Byte *r=table+i*RECORD_SIZE;UInt32 offset=get32(r+16),bytes=get32(r+20);
  if(offset<minimum||offset>start||bytes>start-offset||(i&&offset!=previous)||!checkPayload(self,r,NULL))return 0;
  previous=offset+bytes;
 }
 return previous==start;
}
/* mkdir reserves a sibling on the same drive without overwriting old recovery
   output. A stage is removed only through its exact list of owned files. */
static int lifeStage(const char *destination){
 char parent[260],*slash;unsigned i;
 strcpy(parent,destination);slash=strrchr(parent,LIFE_SEP);if(!slash)return 0;
 slash[1]=0;
 for(i=0;i<10000;i++){
  sprintf(stage,"%sZ7S%04u.TMP",parent,i);
  if(!lifeMkdir(stage)){lifePath(backup,stage,"OLD");if(!lifeMkdir(backup))return 1;rmdir(stage);return 0;}
  if(errno!=EEXIST)return 0;
 }
 return 0;
}
static int lifeRemove(const char *path){
 unsigned attributes;int exists=lifeExists(path);
 if(!exists)return 1;if(exists<0||lifeAttributes(path,&attributes))return 0;
 if(lifeSetAttributes(path,LIFE_WRITABLE))return 0;
 if(!remove(path))return 1;
 if(lifeSetAttributes(path,attributes))printf("WARNING: Could not restore attributes: %s\n",path);
 return 0;
}
static int lifeClean(const char *directory,const Byte *records,unsigned count){
 char path[260];unsigned i;int ok=1;
 for(i=0;i<count;i++){lifePath(path,directory,(const char *)(records+i*RECORD_SIZE));if(!lifeRemove(path))ok=0;}
 lifePath(path,directory,LIFE_MANIFEST);if(!lifeRemove(path))ok=0;return ok;
}
static int lifeDiscard(unsigned oldCount,unsigned newCount){
 int ok=lifeClean(backup,oldTable,oldCount);if(!lifeClean(stage,table,newCount))ok=0;
 if(rmdir(backup))ok=0;if(rmdir(stage))ok=0;
 if(!ok)printf("WARNING: Cleanup incomplete. Recovery files remain in %s\n",stage);
 return ok;
}
static int lifeExtract(FILE *self,unsigned count){
 unsigned i;char path[260];FILE *output;int fd,ok;
 for(i=0;i<count;i++){
  lifePath(path,stage,(const char *)(table+i*RECORD_SIZE));
  fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);if(fd<0)return 0;
  output=fdopen(fd,"wb");if(!output){close(fd);return 0;}
  ok=checkPayload(self,table+i*RECORD_SIZE,output);if(fclose(output))ok=0;if(!ok)return 0;
 }
 return lifeManifestWrite(stage,table,count);
}
static int lifePreflight(const char *destination,unsigned oldCount,unsigned newCount){
 char path[260];unsigned i;int exists;
 for(i=0;i<oldCount;i++){
  lifePath(path,destination,(const char *)(oldTable+i*RECORD_SIZE));exists=lifeExists(path);
  if(exists<0||(exists&&!lifeMatches(path,oldTable+i*RECORD_SIZE))){
   printf("ERROR: Cannot upgrade a modified or unreadable installed file: %s\n",path);return 0;
  }
 }
 for(i=0;i<newCount;i++)if(lifeFind(oldTable,oldCount,(const char *)(table+i*RECORD_SIZE))<0){
  lifePath(path,destination,(const char *)(table+i*RECORD_SIZE));
  if(lifeExists(path)!=0){printf("ERROR: Upgrade would replace an unrelated file: %s\n",path);return 0;}
 }
 return !interrupted;
}
/* This also handles ordinary installation. No payload is published until every
   new file has been checked. The new manifest is the final commit operation. */
static int lifeInstall(FILE *self,const char *destination,unsigned count,int upgrade){
 unsigned oldCount=0,i,j,newMoved=0;Byte moved[MAX_FILES];int oldManifest=0,ownsDir=0,restored=1;
 char source[260],target[260];memset(moved,0,sizeof(moved));
 if(upgrade){
  if(!lifeManifestRead(destination,oldTable,&oldCount)){puts("ERROR: A valid DOS installation record is required for upgrade.");return 2;}
  if(!lifePreflight(destination,oldCount,count))return interrupted?255:2;
 }else if(lifeExists(destination)!=0){puts("ERROR: Destination already exists or cannot be accessed. Use /UPGRADE for a recorded installation.");return 2;}
 if(!lifeStage(destination)){puts("ERROR: Cannot reserve an installation staging directory.");return 2;}
 if(!lifeExtract(self,count)){puts("ERROR: Cannot stage and verify all installation files.");lifeDiscard(0,count);return interrupted?255:2;}
 if(upgrade){
  Byte recordCheck[MAX_FILES*RECORD_SIZE];unsigned checkCount;
  if(!lifeManifestRead(destination,recordCheck,&checkCount)||checkCount!=oldCount||memcmp(recordCheck,oldTable,oldCount*RECORD_SIZE)||!lifePreflight(destination,oldCount,count))goto rollback;
 }else{if(lifeMkdir(destination))goto rollback;ownsDir=1;}
 for(i=0;i<oldCount;i++){
  lifePath(source,destination,(const char *)(oldTable+i*RECORD_SIZE));lifePath(target,backup,(const char *)(oldTable+i*RECORD_SIZE));
  if(interrupted)goto rollback;
  j=(unsigned)lifeExists(source);if(j){if(j!=(unsigned)1||!lifeMatches(source,oldTable+i*RECORD_SIZE)||rename(source,target))goto rollback;moved[i]=1;}
 }
 if(upgrade){lifePath(source,destination,LIFE_MANIFEST);lifePath(target,backup,LIFE_MANIFEST);if(interrupted||rename(source,target))goto rollback;oldManifest=1;}
 for(i=0;i<count;i++){
  lifePath(source,stage,(const char *)(table+i*RECORD_SIZE));lifePath(target,destination,(const char *)(table+i*RECORD_SIZE));
  if(interrupted||rename(source,target))goto rollback;newMoved++;
 }
 lifePath(source,stage,LIFE_MANIFEST);lifePath(target,destination,LIFE_MANIFEST);if(interrupted||rename(source,target))goto rollback;
 puts(upgrade?"Upgrade complete.":"Installation complete. Run " Z7_DOS_MAINEXE " from the installation directory.");
 return lifeDiscard(oldCount,count)?0:1;
 rollback:
 /* Never delete backup output when restoration fails. Both complete copies and
    the manifests remain available at the printed recovery location. */
 while(newMoved){--newMoved;lifePath(source,destination,(const char *)(table+newMoved*RECORD_SIZE));lifePath(target,stage,(const char *)(table+newMoved*RECORD_SIZE));if(rename(source,target))restored=0;}
 for(i=0;i<oldCount;i++)if(moved[i]){lifePath(source,backup,(const char *)(oldTable+i*RECORD_SIZE));lifePath(target,destination,(const char *)(oldTable+i*RECORD_SIZE));if(rename(source,target))restored=0;}
 if(oldManifest){lifePath(source,backup,LIFE_MANIFEST);lifePath(target,destination,LIFE_MANIFEST);if(rename(source,target))restored=0;}
 if(ownsDir&&rmdir(destination))restored=0;
 if(restored){puts("ERROR: Installation stopped; the previous state was restored.");lifeDiscard(oldCount,count);}
 else printf("ERROR: Restoration incomplete. Keep recovery files in %s\n",stage);
 return interrupted?255:2;
}
static int lifeDirectoryEmpty(const char *directory){
#ifdef Z7_DOS_HOST_TEST
 DIR *dir=opendir(directory);struct dirent *entry;int empty=1;if(!dir)return -1;
 while((entry=readdir(dir))!=NULL)if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,"..")){empty=0;break;}
 if(closedir(dir))return -1;return empty;
#else
 struct find_t entry;char path[260];unsigned error;lifePath(path,directory,"*.*");
 error=_dos_findfirst(path,_A_NORMAL|_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_SUBDIR,&entry);
 while(!error){if(strcmp(entry.name,".")&&strcmp(entry.name,".."))return 0;error=_dos_findnext(&entry);}
 return error==2||error==18?1:-1;
#endif
}
static int lifeUninstall(const char *destination){
 unsigned count,i;Byte moved[MAX_FILES];int preserved=0,restored=1,exists,ok;
 char source[260],target[260];memset(moved,0,sizeof(moved));
 if(!lifeManifestRead(destination,oldTable,&count)){puts("ERROR: A valid DOS installation record is required for removal.");return 2;}
 if(!lifeStage(destination)){puts("ERROR: Cannot reserve a removal staging directory.");return 2;}
 for(i=0;i<count;i++){
  lifePath(source,destination,(const char *)(oldTable+i*RECORD_SIZE));exists=lifeExists(source);
  if(exists<0||interrupted)goto rollback;if(!exists)continue;
  if(!lifeMatches(source,oldTable+i*RECORD_SIZE)){
   if(interrupted)goto rollback;
   printf("Preserved modified or unreadable file: %s\n",source);preserved=1;continue;
  }
  lifePath(target,backup,(const char *)(oldTable+i*RECORD_SIZE));if(rename(source,target))goto rollback;moved[i]=1;
 }
 lifePath(source,destination,LIFE_MANIFEST);lifePath(target,backup,LIFE_MANIFEST);
 if(interrupted||rename(source,target))goto rollback;
 ok=lifeDiscard(count,0);
 exists=lifeDirectoryEmpty(destination);
 if(exists==1){if(rmdir(destination)){puts("WARNING: Could not remove the empty installation directory.");ok=0;}}
 else if(!exists){puts("Preserved directory containing remaining files.");preserved=1;}
 else{puts("WARNING: Cannot inspect the remaining installation directory.");ok=0;}
 printf("Removal complete%s\n",preserved?"; modified or unrelated files preserved.":".");return ok?0:1;
 rollback:
 for(i=0;i<count;i++)if(moved[i]){lifePath(source,backup,(const char *)(oldTable+i*RECORD_SIZE));lifePath(target,destination,(const char *)(oldTable+i*RECORD_SIZE));if(rename(source,target))restored=0;}
 if(restored){puts("ERROR: Removal stopped; installed files were restored.");lifeDiscard(count,0);}
 else printf("ERROR: Removal restoration incomplete. Keep recovery files in %s\n",stage);
 return interrupted?255:2;
}
#ifndef Z7_DOS_HOST_TEST
int main(int argc,char **argv){
 const char *argument=NULL;char destination[260],selfPath[260],root[4],*slash;
 FILE *self=NULL;unsigned i,count;int quiet=0,upgrade=0,uninstall=0,result;
#ifdef Z7_DOS_UNINSTALL
 uninstall=1;
#endif
 puts("7-Zip 26.05 DOS Setup v1.0");interrupted=0;CrcGenerateTable();signal(SIGINT,cancelSetup);
 for(i=1;i<(unsigned)argc;i++){
  if(!stricmp(argv[i],"/S"))quiet=1;
  else if(!stricmp(argv[i],"/UPGRADE"))upgrade=1;
  else if(!stricmp(argv[i],"/UNINSTALL"))uninstall=1;
  else if(!strcmp(argv[i],"/?")){
   puts("Usage: " Z7_DOS_SETUPNAME " [directory] [/UPGRADE | /UNINSTALL] [/S]\n       UNINST [directory] [/S]\nDefault installation directory: C:\\7-Zip\nExit codes: 0 success, 1 declined/cleanup warning, 2 error, 7 usage, 255 cancelled.");return 0;
  }else if(argv[i][0]=='/'||argument){puts("ERROR: Unknown option or too many directories.");return 7;}
  else argument=argv[i];
 }
 if(upgrade&&uninstall){puts("ERROR: Choose upgrade or uninstall, not both.");return 7;}
 if(!_fullpath(selfPath,argv[0],sizeof(selfPath))){puts("ERROR: Cannot resolve the executable path.");return 2;}
#ifdef Z7_DOS_UNINSTALL
 if(!argument){slash=strrchr(selfPath,'\\');if(!slash)return 7;*slash=0;argument=selfPath;}
#endif
 if(!argument)argument="C:\\7-Zip";
 if(!_fullpath(destination,argument,sizeof(destination))||strlen(destination)>230||!validDestination(destination)||destination[1]!=':'||destination[2]!='\\'){
  puts("ERROR: Use a DOS 8.3 directory with an existing parent.");return 7;
 }
 if(!uninstall){self=fopen(selfPath,"rb");if(!self||!lifePayload(self,&count)){
  if(self)fclose(self);puts("ERROR: Installer payload is missing or damaged. No files were changed.");return interrupted?255:2;
 }}
 printf("Destination: %s\n",destination);
 if(!quiet){int answer;printf("%s now? [Y/N] ",uninstall?"Uninstall":(upgrade?"Upgrade":"Install"));answer=getchar();if(answer!='y'&&answer!='Y'){if(self)fclose(self);return 1;}}
 /* Neither the shell's current directory nor the DOS per-drive working
    directory may keep the installation folder open during removal. */
 root[0]=destination[0];root[1]=':';root[2]='\\';root[3]=0;
 if(uninstall&&chdir(root)){puts("ERROR: Cannot leave the installation directory.");return 2;}
 result=uninstall?lifeUninstall(destination):lifeInstall(self,destination,count,upgrade);
 if(self&&fclose(self)&&!result)result=2;
 return result;
}
#endif
