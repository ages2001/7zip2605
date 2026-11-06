/* Native integration fixture; not part of the installed application payload. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "associations.h"
int PASCAL WinMain(HINSTANCE current,HINSTANCE previous,LPSTR command,int show){
 FILE *log;int ok=1,present;char value[260],args[40];const char *exe="C:\\7-Zip\\7ZFM16.EXE";
 (void)current;(void)previous;(void)show;
 if(_fstrlen(command)>=sizeof(args))return 7;_fstrcpy(args,command);
 log=fopen(!stricmp(args,"CHECK")?"C:\\ASCK47.LOG":"C:\\AS47.LOG","w");if(!log)return 2;
 if(!stricmp(args,"SEED")){
  ok=WriteProfileString("Extensions","7z","OLDAPP.EXE ^.7z")&&WriteProfileString("Extensions","zip",NULL);
  ok&=assocSet(exe,"7z",1)&&assocSet(exe,"zip",1)&&assocCurrent(exe,"7z")&&assocCurrent(exe,"zip");
  ok&=WriteProfileString("Extensions","zip","USERAPP.EXE ^.zip");
 }else if(!stricmp(args,"INSTALL"))ok=assocSet(exe,"7z",1)&&assocSet(exe,"zip",1);
 else if(!stricmp(args,"CHECK")){
  ok=assocRead("Extensions","7z",value,&present)&&present&&!strcmp(value,"OLDAPP.EXE ^.7z");
  ok&=assocRead("Extensions","zip",value,&present)&&present&&!strcmp(value,"USERAPP.EXE ^.zip");
  ok&=GetProfileString(ASSOC_SECTION,NULL,"",value,sizeof(value))==0;
 }else if(!stricmp(args,"ROOT"))ok=assocSet("C:\\7ZFM16.EXE","7z",1)&&assocSet("C:\\7ZFM16.EXE","zip",1);
 else if(!stricmp(args,"OFF"))ok=assocRemoveAll("C:\\7ZFM16.EXE");
 else if(!stricmp(args,"PROFILE")){
  unsigned n=GetProfileString("7ZIP-NO-SUCH-SECTION","Missing","\001",value,sizeof(value));fprintf(log,"Control default: count=%u first-byte=%u\n",n,(unsigned)(unsigned char)value[0]);
 }else ok=0;
 fprintf(log,"%s native association %s\n",ok?"PASS":"FAIL",args);fclose(log);return ok?0:2;
}
