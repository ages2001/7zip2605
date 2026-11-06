/* Windows 3.0-compatible classic DDE. No DDEML or Win32 dependency. */
#ifndef Z7_WIN16_PROGMAN_H
#define Z7_WIN16_PROGMAN_H
#include <dde.h>
#include "group_snapshot.h"
static HWND progmanClient,progmanServer;
static int progmanInitializing,progmanComplete,progmanAccepted;
static HGLOBAL progmanCommand;
static ATOM progmanItem;
static char progmanData[8192];
static int progmanSnapshotError;
/* Windows 3.0 Program Manager does not accept quoted DDE arguments. All
   interpolated paths are restricted to DOS names without DDE delimiters. */
static const char progmanGroup[]="7-Zip 26.05 - 16-bit";
static const char *progmanError="Program Manager did not accept a connection.";
static void progmanPump(void){MSG msg;while(PeekMessage(&msg,NULL,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}}
LRESULT FAR PASCAL __export ProgmanProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 if(message==WM_DDE_ACK){
  if(progmanInitializing){
   if(!progmanServer)progmanServer=(HWND)wParam;
   else if(progmanServer!=(HWND)wParam)PostMessage((HWND)wParam,WM_DDE_TERMINATE,(WPARAM)window,0);
   GlobalDeleteAtom(LOWORD(lParam));GlobalDeleteAtom(HIWORD(lParam));
  }else if((HWND)wParam==progmanServer){
   if(progmanCommand&&(HGLOBAL)HIWORD(lParam)==progmanCommand){progmanAccepted=(LOWORD(lParam)&0x8000)!=0;progmanComplete=1;}
   else if(progmanItem&&HIWORD(lParam)==progmanItem){progmanAccepted=0;progmanComplete=1;}
  }
  return 0;
 }
 if(message==WM_DDE_DATA){
  HGLOBAL handle=(HGLOBAL)LOWORD(lParam);DDEDATA FAR *data=(DDEDATA FAR *)GlobalLock(handle);
  BOOL release=FALSE,ack=FALSE;DWORD size=GlobalSize(handle);unsigned n;
  if(data){
   release=data->fRelease;ack=data->fAckReq;
   if((HWND)wParam==progmanServer&&HIWORD(lParam)==progmanItem&&data->cfFormat==CF_TEXT&&size>4&&size-4<sizeof(progmanData)){
    n=(unsigned)(size-4);_fmemcpy(progmanData,data->Value,n);progmanData[n]=0;
    progmanAccepted=1;progmanComplete=1;
   }
   GlobalUnlock(handle);
  }
  if(release)GlobalFree(handle);
  if(ack)PostMessage((HWND)wParam,WM_DDE_ACK,(WPARAM)window,MAKELONG(0x8000,HIWORD(lParam)));
  else GlobalDeleteAtom(HIWORD(lParam));
  return 0;
 }
 if(message==WM_DDE_TERMINATE){
  if((HWND)wParam==progmanServer){progmanServer=NULL;PostMessage((HWND)wParam,WM_DDE_TERMINATE,(WPARAM)window,0);}
  return 0;
 }
 return DefWindowProc(window,message,wParam,lParam);
}
static int progmanWait(void){
 DWORD start=GetTickCount();while(!progmanComplete&&progmanServer&&(DWORD)(GetTickCount()-start)<10000UL){progmanPump();Yield();}
 return progmanComplete&&progmanAccepted;
}
static int progmanConnect(HINSTANCE instance){
 WNDCLASS cls;ATOM atom;DWORD start,next;
 memset(&cls,0,sizeof(cls));cls.lpfnWndProc=ProgmanProc;cls.hInstance=instance;cls.lpszClassName="7Zip16DDE";
 if(!RegisterClass(&cls))return 0;
 progmanClient=CreateWindow("7Zip16DDE","",WS_POPUP,0,0,0,0,NULL,NULL,instance,NULL);if(!progmanClient)return 0;
 atom=GlobalAddAtom("PROGMAN");progmanInitializing=1;start=GetTickCount();
 do {
  SendMessage((HWND)-1,WM_DDE_INITIATE,(WPARAM)progmanClient,MAKELONG(atom,atom));
  next=GetTickCount();while(!progmanServer&&(DWORD)(GetTickCount()-next)<250UL){progmanPump();Yield();}
 }while(!progmanServer&&(DWORD)(GetTickCount()-start)<10000UL);
 progmanInitializing=0;GlobalDeleteAtom(atom);return progmanServer!=NULL;
}
static int progmanExecute(const char *command){
 char FAR *data;int ok;unsigned size=(unsigned)strlen(command)+1;
 if(!progmanServer||progmanCommand)return 0;
 progmanCommand=GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE,size);if(!progmanCommand)return 0;
 data=(char FAR *)GlobalLock(progmanCommand);if(!data){GlobalFree(progmanCommand);progmanCommand=NULL;return 0;}
 _fmemcpy(data,command,size);GlobalUnlock(progmanCommand);progmanComplete=progmanAccepted=0;
 if(!PostMessage(progmanServer,WM_DDE_EXECUTE,(WPARAM)progmanClient,MAKELONG(0,progmanCommand))){GlobalFree(progmanCommand);progmanCommand=NULL;return 0;}
 ok=progmanWait();
 /* A timed-out server may still be reading the command. Do not free its
    shared block before acknowledgement or conversation termination. */
 if(progmanComplete){GlobalFree(progmanCommand);progmanCommand=NULL;}
 return ok;
}
static int progmanFileSnapshot(void){
 char ini[260],path[260],keys[4096],*key;unsigned length;FILE *file;int result;
 progmanData[0]=0;progmanSnapshotError=0;
 length=GetWindowsDirectory(ini,sizeof(ini));if(!length||length>sizeof(ini)-14){progmanSnapshotError=1;return 0;}
 strcat(ini,"\\PROGMAN.INI");
 length=GetPrivateProfileString("Groups",NULL,"",keys,sizeof(keys),ini);
 if(length>=sizeof(keys)-2){progmanSnapshotError=1;return 0;}
 for(key=keys;*key;key+=strlen(key)+1){
  length=GetPrivateProfileString("Groups",key,"",path,sizeof(path),ini);
  if(!length||length>=sizeof(path)-1){progmanSnapshotError=1;return 0;}
  file=fopen(path,"rb");if(!file){progmanSnapshotError=1;return 0;}
  result=groupSnapshot(file,progmanGroup,progmanData,sizeof(progmanData));fclose(file);
  if(result<0){progmanSnapshotError=1;return 0;}if(result)return 1;
 }
 return 0;
}
static int progmanRequest(void){
 int ok;progmanData[0]=0;progmanItem=GlobalAddAtom(progmanGroup);progmanComplete=progmanAccepted=0;
 progmanSnapshotError=0;
 if((GetVersion()&65535UL)==3){GlobalDeleteAtom(progmanItem);progmanItem=0;return progmanFileSnapshot();}
 if(!PostMessage(progmanServer,WM_DDE_REQUEST,(WPARAM)progmanClient,MAKELONG(CF_TEXT,progmanItem))){GlobalDeleteAtom(progmanItem);progmanItem=0;return 0;}
 ok=progmanWait();
 /* Negative ACK leaves the request atom owned by the requester. */
 if(progmanComplete&&!progmanAccepted)GlobalDeleteAtom(progmanItem);
 progmanItem=0;return ok;
}
static int progmanOwns(const char *title,const char *target){
 const char *line=progmanData,*end,*name,*nameEnd,*value,*valueEnd;
 while(*line){
  end=strchr(line,'\n');if(!end)end=line+strlen(line);name=line;
  if(name<end&&*name=='"'){name++;nameEnd=memchr(name,'"',end-name);}else nameEnd=memchr(name,',',end-name);
  if(nameEnd&&(unsigned)(nameEnd-name)==strlen(title)&&!strnicmp(name,title,nameEnd-name)){
   value=nameEnd;if(value<end&&*value=='"')value++;
   if(value>=end||*value++!=',')return 0;
   if(value<end&&*value=='"'){value++;valueEnd=memchr(value,'"',end-value);}else valueEnd=memchr(value,',',end-value);
   return valueEnd&&(unsigned)(valueEnd-value)==strlen(target)&&!strnicmp(value,target,valueEnd-value);
  }
  line=*end?end+1:end;
 }
 return 0;
}
static void progmanDisconnect(void){
 DWORD start=GetTickCount();if(progmanServer)PostMessage(progmanServer,WM_DDE_TERMINATE,(WPARAM)progmanClient,0);
 while(progmanServer&&(DWORD)(GetTickCount()-start)<2000UL){progmanPump();Yield();}
 if(!progmanServer&&progmanCommand){GlobalFree(progmanCommand);progmanCommand=NULL;}
 if(progmanClient)DestroyWindow(progmanClient);progmanClient=NULL;
}
static int progmanShortcuts(HINSTANCE instance,const char *directory,int removing){
 const char *titles[2]={"7-Zip File Manager","Uninstall 7-Zip"};
 const char *names[2]={"7ZFM16.EXE","UNINST16.EXE"};
 char target[260],command[640];int ok=1;unsigned i;
 if(!progmanConnect(instance)){progmanDisconnect();return 0;}
 if(removing&&(GetVersion()&65535UL)==3){
  /* Windows 3.0 implements DeleteGroup, but not DeleteItem. Delete only
     groups containing exclusively our targets. Preserve modified groups. */
  const char *line;char first[320],second[320];int owned=1;
  if(!progmanRequest()){progmanError="The Program Manager group could not be verified.";progmanDisconnect();return 0;}
  sprintf(first,"\"%s\",\"%s\\%s\",",titles[0],directory,names[0]);
  sprintf(second,"\"%s\",\"%s\\%s\",",titles[1],directory,names[1]);
  line=strchr(progmanData,'\n');if(!line)owned=0;
  else for(line++;*line;){
   const char *end=strchr(line,'\n');if(!end)end=line+strlen(line);
   if(end>line&&*line!='\r'&&strnicmp(line,first,strlen(first))&&strnicmp(line,second,strlen(second)))owned=0;
   line=*end?end+1:end;
  }
  if(!owned){progmanError="The Windows 3.0 group contains user-edited items and was preserved.";progmanDisconnect();return 0;}
  sprintf(command,"[DeleteGroup(%s)]",progmanGroup);ok=progmanExecute(command);
  if(ok&&(progmanRequest()||progmanSnapshotError))ok=0;
  progmanDisconnect();return ok;
 }
 if(!removing){
  /* Do not replace an existing group or its user-edited items. */
  if(progmanRequest()||progmanSnapshotError){progmanError="An existing or unreadable Program Manager group was preserved.";progmanDisconnect();return 0;}
  progmanError="Program Manager did not accept CreateGroup.";
  sprintf(command,"[CreateGroup(%s)]",progmanGroup);ok=progmanExecute(command);
 }
 sprintf(command,"[ShowGroup(%s,1)]",progmanGroup);
 if(ok){progmanError="Program Manager did not accept ShowGroup.";ok=progmanExecute(command);}
 for(i=0;i<2&&ok;i++){
  sprintf(target,"%s\\%s",directory,names[i]);
  if(removing){
   if(!progmanRequest()){ok=0;break;}
   if(!progmanOwns(titles[i],target))continue;
   sprintf(command,"[DeleteItem(%s)]",titles[i]);
  }else sprintf(command,"[AddItem(%s,%s,%s,0)]",target,titles[i],target);
  progmanError=removing?"Program Manager did not accept DeleteItem.":"Program Manager did not accept AddItem.";
  ok=progmanExecute(command);
 }
 if(ok&&removing&&progmanRequest()){
  const char *p=strchr(progmanData,'\n');if(p){p++;while(*p=='\r'||*p=='\n')p++;if(!*p){sprintf(command,"[DeleteGroup(%s)]",progmanGroup);ok=progmanExecute(command);}}
 }
 if(ok&&!removing){
  progmanError="Program Manager shortcut verification failed.";
  for(i=0;i<2;i++){sprintf(target,"%s\\%s",directory,names[i]);if(!progmanRequest()||!progmanOwns(titles[i],target))ok=0;}
 }
 progmanDisconnect();return ok;
}
#endif
