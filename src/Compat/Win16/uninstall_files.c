/* DOS removal helper. It runs after the Win16 uninstall task exits, so the
   application executables are no longer held open by Windows. */
#define main unusedSetupMain
#include "../DOS/setup.c"
#undef main
#include <dos.h>
static int installedFileMatches(const char *path,const Byte *record){
 FILE *file=fopen(path,"rb");UInt32 size=get32(record+20),crc=CRC_INIT_VAL;int ok;
 if(!file)return 0;
 while(size){size_t n=size>sizeof(buffer)?sizeof(buffer):(size_t)size;
  if(fread(buffer,1,n,file)!=n){fclose(file);return 0;}crc=CrcUpdate(crc,buffer,n);size-=n;}
 ok=fgetc(file)==EOF&&!ferror(file)&&CRC_GET_DIGEST(crc)==get32(record+24);fclose(file);return ok;
}
int main(int argc,char **argv){
 Byte header[20];char path[260],manifest[260],driveRoot[4];FILE *file;const char *logPath;unsigned count,size,i,j;int ok=1,preserved=0;struct stat info;
 if(argc!=3||strcmp(argv[2],"/S")||strlen(argv[1])>230||!validDestination(argv[1])||argv[1][1]!=':'||argv[1][2]!='\\')return 7;
 driveRoot[0]=argv[1][0];driveRoot[1]=':';driveRoot[2]='\\';driveRoot[3]=0;
 if(chdir(driveRoot))return 2; /* DOS cannot remove its current directory. */
 sprintf(manifest,"%s\\INSTALL.DAT",argv[1]);file=fopen(manifest,"rb");if(!file)return 2;
 if(fread(header,1,20,file)!=20||memcmp(header,"7Z16I33\0",8)||get32(header+8)>MAX_FILES){fclose(file);return 2;}
 count=(unsigned)get32(header+8);size=count*RECORD_SIZE;
 if(!count||get32(header+12)!=size||fread(table,1,size,file)!=size||fgetc(file)!=EOF||ferror(file)){fclose(file);return 2;}
 fclose(file);CrcGenerateTable();if(CrcCalc(table,size)!=get32(header+16))return 2;
 for(i=0;i<count;i++){
  const char *name=(const char *)(table+i*RECORD_SIZE);
  if(!memchr(name,0,13)||!validName(name)||!stricmp(name,"INSTALL.DAT"))return 2;
  for(j=0;j<i;j++)if(!stricmp(name,(const char *)(table+j*RECORD_SIZE)))return 2;
 }
 logPath=getenv("Z7UNLOG");file=logPath&&*logPath?fopen(logPath,"w"):stdout;if(!file)return 2;
 for(i=0;i<count;i++){
  const Byte *record=table+i*RECORD_SIZE;sprintf(path,"%s\\%s",argv[1],record);
  if(stat(path,&info)){if(errno!=ENOENT)ok=0;continue;}
  if(!installedFileMatches(path,record)){fprintf(file,"Preserved modified file: %s\n",path);preserved=1;continue;}
  {unsigned attributes;
   if(_dos_getfileattr(path,&attributes)||_dos_setfileattr(path,0)){fprintf(file,"ERROR: Could not access %s\n",path);ok=0;}
   else if(remove(path)){
    if(_dos_setfileattr(path,attributes))fprintf(file,"WARNING: Could not restore attributes of %s\n",path);
    fprintf(file,"ERROR: Could not remove %s\n",path);ok=0;
   }else fprintf(file,"Removed %s\n",path);
  }
 }
 if(ok){
  if(remove(manifest)){ok=0;fputs("ERROR: Could not remove installation record.\n",file);}
  else if(rmdir(argv[1])){
   struct find_t entry;unsigned found;int remaining=0;
   sprintf(path,"%s\\*.*",argv[1]);found=_dos_findfirst(path,_A_NORMAL|_A_HIDDEN|_A_SYSTEM|_A_SUBDIR,&entry);
   while(!found){if(strcmp(entry.name,".")&&strcmp(entry.name,"..")){remaining=1;break;}found=_dos_findnext(&entry);}
   if(remaining){preserved=1;fputs("Preserved directory containing remaining files.\n",file);}
   else {ok=0;fputs("ERROR: Could not remove the empty installation directory.\n",file);}
  }
 }
 fprintf(file,"%s removal%s\n",ok?"PASS":"FAIL",preserved?"; modified or unrelated files preserved":"");if(file!=stdout)fclose(file);
 return ok?0:2;
}
