/* Native DOS installer. Payloads and their checksums are appended by pack_setup.py.
   Only a new directory is accepted; failures remove files created by this run. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef Z7_DOS_HOST_TEST
#include <unistd.h>
#else
#include <io.h>
#include <direct.h>
#endif
#include <signal.h>
#include "../../C/7zCrc.h"
#ifndef Z7_SETUP_PUMP
#define Z7_SETUP_PUMP() ((void)0)
#endif
#ifndef Z7_SETUP_FINISH
#define Z7_SETUP_FINISH(path,records,count) 1
#endif
#ifndef Z7_SETUP_TITLE
#define Z7_SETUP_TITLE "7-Zip 26.05 DOS Setup v1.0"
#endif
#ifndef Z7_SETUP_SUCCESS
#define Z7_SETUP_SUCCESS "Installation complete. Run 7Z.EXE from the installation directory."
#endif
#define MAX_FILES 32
#define RECORD_SIZE 28
static Byte table[MAX_FILES*RECORD_SIZE],buffer[4096];
static volatile sig_atomic_t interrupted;
#ifndef __WINDOWS__
static void cancelSetup(int sig){(void)sig;interrupted=1;}
#endif
static UInt32 get32(const Byte *p){return (UInt32)p[0]|((UInt32)p[1]<<8)|((UInt32)p[2]<<16)|((UInt32)p[3]<<24);}
static int validName(const char *p){
 unsigned base=0,ext=0,dot=0;char name[9];
 for(;*p;p++){
  unsigned ch=(unsigned char)*p;
  if(ch=='.'){if(dot||!base)return 0;dot=1;continue;}
  if(!isalnum(ch)&&ch!='-'&&ch!='_')return 0;
  if(dot){if(++ext>3)return 0;}else{if(base==8)return 0;name[base++]=(char)toupper(ch);}
 }
 name[base]=0;
 if(!base||(dot&&!ext)||!strcmp(name,"CON")||!strcmp(name,"PRN")||!strcmp(name,"AUX")||!strcmp(name,"NUL"))return 0;
 if(base==4&&name[3]>='0'&&name[3]<='9'&&(!memcmp(name,"COM",3)||!memcmp(name,"LPT",3)))return 0;
 return 1;
}
static int validDestination(const char *path){
 char component[13];unsigned n;
 if(isalpha((unsigned char)path[0])&&path[1]==':')path+=2;
 if(*path=='\\'||*path=='/')path++;
 if(!*path)return 0;
 for(;;){
  n=0;while(*path&&*path!='\\'&&*path!='/'){if(n==12)return 0;component[n++]=*path++;}
  component[n]=0;if(!validName(component))return 0;
  if(!*path)return 1;path++;
 }
}
static int checkPayload(FILE *self,const Byte *record,FILE *output){
 UInt32 left=get32(record+20),crc=CRC_INIT_VAL;
 if(fseek(self,(long)get32(record+16),SEEK_SET))return 0;
 while(left){
  size_t n=left>sizeof(buffer)?sizeof(buffer):(size_t)left;
  Z7_SETUP_PUMP();
  if(interrupted||fread(buffer,1,n,self)!=n)return 0;
  crc=CrcUpdate(crc,buffer,n);
  if(output&&fwrite(buffer,1,n,output)!=n)return 0;
  left-=n;
 }
 return CRC_GET_DIGEST(crc)==get32(record+24);
}
#ifndef Z7_SETUP_HELPERS_ONLY
int main(int argc,char **argv){
 FILE *self=NULL,*output=NULL;Byte footer[24];long length;UInt32 start,count,size;
 unsigned i,j,created=0;int quiet=0,result=2,fd,ownsDir=0,haveDestination=0;
 const char *destination="C:\\7-Zip";char path[260],names[MAX_FILES][13];
 interrupted=0;puts(Z7_SETUP_TITLE);
 for(i=1;i<(unsigned)argc;i++){
  if(!strcmp(argv[i],"/S")||!strcmp(argv[i],"/s"))quiet=1;
  else if(!strcmp(argv[i],"/?")){puts("Usage: 7ZSETDOS [new-directory] [/S]");return 0;}
  else if(haveDestination){puts("ERROR: Too many destination arguments.");return 7;}
  else {destination=argv[i];haveDestination=1;}
 }
 if(strlen(destination)>230||!validDestination(destination)){puts("ERROR: Enter a new DOS 8.3 directory. Its parent must exist.");return 7;}
 self=fopen(argv[0],"rb");if(!self){puts("ERROR: Cannot open the installer executable.");goto done;}
 if(fseek(self,0,SEEK_END)||(length=ftell(self))<24||fseek(self,length-24,SEEK_SET)||fread(footer,1,24,self)!=24)goto damaged;
 if(memcmp(footer,"7ZDS31\0\0",8))goto damaged;
 start=get32(footer+8);count=get32(footer+12);size=get32(footer+16);
 if(!count||count>MAX_FILES||size!=count*RECORD_SIZE||start>(UInt32)length-24||size!=(UInt32)length-24-start)goto damaged;
 if(fseek(self,(long)start,SEEK_SET)||fread(table,1,(size_t)size,self)!=(size_t)size)goto damaged;
 CrcGenerateTable();if(CrcCalc(table,(size_t)size)!=get32(footer+20))goto damaged;
 for(i=0;i<count;i++){
  const Byte *r=table+i*RECORD_SIZE;UInt32 offset=get32(r+16),bytes=get32(r+20);
  if(!memchr(r,0,13)||!validName((const char *)r)||offset>start||bytes>start-offset)goto damaged;
  strcpy(names[i],(const char *)r);for(j=0;names[i][j];j++)names[i][j]=(char)toupper((unsigned char)names[i][j]);
  for(j=0;j<i;j++)if(!strcmp(names[j],names[i]))goto damaged;
  if(!checkPayload(self,r,NULL))goto damaged;
 }
 printf("Destination: %s\n",destination);
 if(!quiet){int answer;fputs("Install now? [Y/N] ",stdout);answer=getchar();if(answer!='y'&&answer!='Y'){result=1;goto done;}}
 /* Native Windows cancellation is delivered by the dialog's message pump.
    The DOS signal runtime unnecessarily imports WIN87EM in a Windows build. */
#ifndef __WINDOWS__
 signal(SIGINT,cancelSetup);
#endif
 if(mkdir(destination)){puts("ERROR: Cannot create the destination. Existing directories are never overwritten.");goto done;}
 ownsDir=1;
 for(i=0;i<count;i++){
  sprintf(path,"%s\\%s",destination,names[i]);
  fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);if(fd<0)goto failed;
  created=i+1;output=fdopen(fd,"wb");if(!output){close(fd);goto failed;}
  if(!checkPayload(self,table+i*RECORD_SIZE,output))goto failed;
  if(fclose(output)){output=NULL;goto failed;}output=NULL;
  printf("Installed %s\n",names[i]);
 }
 if(!Z7_SETUP_FINISH(destination,table,count))goto failed;
 result=0;puts(Z7_SETUP_SUCCESS);goto done;
 damaged:puts("ERROR: Installer payload is missing or damaged. No files were installed.");goto done;
 failed:puts(interrupted?"Installation cancelled.":"ERROR: Installation failed. Removing incomplete output.");
 done:
 if(output)fclose(output);if(self)fclose(self);
 if(result&&ownsDir){
  for(i=0;i<created;i++){sprintf(path,"%s\\%s",destination,names[i]);if(remove(path))printf("WARNING: Could not remove %s\n",path);}
  if(rmdir(destination))puts("WARNING: Could not remove the incomplete installation directory.");
 }
 return result;
}
#endif
