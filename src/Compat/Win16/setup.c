/* Native Win16 setup and uninstall launcher, including Windows 3.0 real mode. */
#include "../../C/7zCrc.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#undef NULL
#define NULL 0
static HINSTANCE setupInstance;
static HWND setupDialog;
static int setupBusy,setupSilent,setupResult=1;
static char setupLog[4096],setupDirectory[232]="C:\\7-Zip",setupExecutable[260];
static void setupPump(void);
static void setupLogLine(const char *text){
 unsigned used=(unsigned)strlen(setupLog),length=(unsigned)strlen(text);
 if(used+length+3<sizeof(setupLog)){strcat(setupLog,text);strcat(setupLog,"\r\n");}
 if(setupDialog)SetDlgItemText(setupDialog,502,text);
}
static int setupPuts(const char *text){setupLogLine(text);return 0;}
static int setupPrintf(const char *format,...){
 char text[512];va_list args;va_start(args,format);vsprintf(text,format,args);va_end(args);
 if(text[0]&&text[strlen(text)-1]=='\n')text[strlen(text)-1]=0;setupLogLine(text);return 0;
}
#include "install_manifest.h"
#define Z7_SETUP_PUMP() setupPump()
#define Z7_SETUP_FINISH(directory,records,count) writeInstallManifest(directory,records,(unsigned)(count))
#define Z7_SETUP_TITLE "7-Zip 26.05 Windows 3.0/3.1x Setup v1.0"
#define Z7_SETUP_SUCCESS "Application files installed."
#define main embeddedSetupMain
#define puts setupPuts
#define printf setupPrintf
#include "../DOS/setup.c"
#undef main
#undef puts
#undef printf
#include "progman.h"
#include "associations.h"
static void setupPump(void){
 MSG message;
 while(PeekMessage(&message,NULL,0,0,PM_REMOVE)){
  if(!setupDialog||!IsDialogMessage(setupDialog,&message)){TranslateMessage(&message);DispatchMessage(&message);}
 }
}
static void saveSetupLog(void){
 const char *path=getenv("Z7SETLOG");FILE *file;
 if(!path||!*path)return;
 file=fopen(path,"wb");if(file){fputs(setupLog,file);fclose(file);}
}
#ifdef Z7_UNINSTALL
static int installedManagerRunning(void){
 HWND window=GetWindow(GetDesktopWindow(),GW_CHILD);char name[40],path[260],expected[260];unsigned length;
 sprintf(expected,"%s\\7ZFM16.EXE",setupDirectory);
 while(window){
  if(GetClassName(window,name,sizeof(name))&&!stricmp(name,"Z7Win16Browser")){
   HINSTANCE instance=(HINSTANCE)GetWindowWord(window,GWW_HINSTANCE);
   length=GetModuleFileName(instance,path,sizeof(path));
   if(!length||length>=sizeof(path)-1||!stricmp(path,expected)){
    setupLogLine("ERROR: Close this installation's 7-Zip File Manager before uninstalling. No application files were removed.");saveSetupLog();
    if(!setupSilent)MessageBox(NULL,setupLog,"Uninstall 7-Zip",MB_OK|MB_ICONEXCLAMATION);
    return 1;
   }
  }
  window=GetWindow(window,GW_HWNDNEXT);
 }
 return 0;
}
#endif
static int validateInstallManifest(void){
 char path[260];Byte header[20];FILE *file;UInt32 count,size;unsigned i,j;int ok;
 sprintf(path,"%s\\INSTALL.DAT",setupDirectory);file=fopen(path,"rb");if(!file)return 0;
 ok=fread(header,1,20,file)==20&&!memcmp(header,"7Z16I33\0",8);
 if(!ok){fclose(file);return 0;}
 count=get32(header+8);size=get32(header+12);
 if(!count||count>MAX_FILES||size!=count*RECORD_SIZE){fclose(file);return 0;}
 ok=fread(table,1,(unsigned)size,file)==size&&fgetc(file)==EOF&&!ferror(file);fclose(file);
 CrcGenerateTable();if(!ok||CrcCalc(table,(unsigned)size)!=get32(header+16))return 0;
 for(i=0;i<count;i++){
  const char *name=(const char *)(table+i*RECORD_SIZE);
  if(!memchr(name,0,13)||!validName(name)||!stricmp(name,"INSTALL.DAT"))return 0;
  for(j=0;j<i;j++)if(!stricmp(name,(const char *)(table+j*RECORD_SIZE)))return 0;
 }
 return 1;
}
static void performSetup(void){
 char *args[3];int shortcuts;
 setupBusy=1;EnableWindow(GetDlgItem(setupDialog,IDOK),FALSE);EnableWindow(GetDlgItem(setupDialog,501),FALSE);
 args[0]=setupExecutable;args[1]=setupDirectory;args[2]="/S";
 setupResult=embeddedSetupMain(3,args);
 if(!setupResult){
  EnableWindow(GetDlgItem(setupDialog,IDCANCEL),FALSE);
  shortcuts=progmanShortcuts(setupInstance,setupDirectory,0);
  if(shortcuts)setupLogLine("Program Manager shortcuts verified: File Manager and Uninstall.");
  else {setupResult=1;setupLogLine(progmanError);setupLogLine("WARNING: Application files are installed, but Program Manager shortcuts could not be created. Run UNINST16.EXE to uninstall.");}
 }
 setupBusy=0;saveSetupLog();
 if(!setupSilent)MessageBox(setupDialog,setupLog,"7-Zip Setup",MB_OK|(setupResult?MB_ICONEXCLAMATION:MB_ICONINFORMATION));
 EndDialog(setupDialog,setupResult);
}
BOOL FAR PASCAL __export SetupProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 (void)lParam;
 switch(message){
 case WM_INITDIALOG:setupDialog=window;SetDlgItemText(window,501,setupDirectory);SendDlgItemMessage(window,501,EM_LIMITTEXT,230,0);if(setupSilent)PostMessage(window,WM_COMMAND,IDOK,0);return TRUE;
 case WM_COMMAND:
  if(wParam==IDOK&&!setupBusy){
   GetDlgItemText(window,501,setupDirectory,sizeof(setupDirectory));
   if(!validDestination(setupDirectory)||strlen(setupDirectory)<4||setupDirectory[1]!=':'||setupDirectory[2]!='\\'){
    if(setupSilent){setupResult=7;EndDialog(window,7);}else MessageBox(window,"Enter an absolute DOS 8.3 path, such as C:\\7-Zip. The parent directory must exist.","7-Zip Setup",MB_OK|MB_ICONEXCLAMATION);return TRUE;
   }
   performSetup();return TRUE;
  }
  if(wParam==IDCANCEL){if(setupBusy)interrupted=1;else EndDialog(window,1);return TRUE;}break;
 case WM_CLOSE:if(setupBusy)interrupted=1;else EndDialog(window,1);return TRUE;
 }
 return FALSE;
}
int PASCAL WinMain(HINSTANCE current,HINSTANCE previous,LPSTR command,int show){
 FARPROC proc;unsigned length;char arguments[260];(void)previous;(void)show;
 setupInstance=current;length=GetModuleFileName(current,setupExecutable,sizeof(setupExecutable));
 if(!length||length>=sizeof(setupExecutable)-1)return 2;
 if(_fstrlen(command)>=sizeof(arguments))return 7;_fstrcpy(arguments,command);
#ifdef Z7_UNINSTALL
 {
  char *slash=strrchr(setupExecutable,'\\');char launch[520];int answer;
  if(!slash||slash-setupExecutable>=sizeof(setupDirectory))return 2;
  memcpy(setupDirectory,setupExecutable,slash-setupExecutable);setupDirectory[slash-setupExecutable]=0;
  if(!validDestination(setupDirectory)||setupDirectory[1]!=':'||setupDirectory[2]!='\\')return 7;
  setupSilent=!stricmp(arguments,"/S");
  if(!validateInstallManifest()){MessageBox(NULL,"The installation record is missing or damaged. No files were removed.","Uninstall 7-Zip",MB_OK|MB_ICONEXCLAMATION);return 2;}
  answer=setupSilent?IDYES:MessageBox(NULL,"Uninstall 7-Zip? Modified files and unrelated files will be preserved.","Uninstall 7-Zip",MB_YESNO|MB_ICONQUESTION);
  if(answer!=IDYES)return 1;
  if(installedManagerRunning())return 2;
  sprintf(launch,"%s\\7ZFM16.EXE",setupDirectory);
  if(!assocRemoveAll(launch)){
   setupLogLine("ERROR: File associations could not be restored. No application files were removed. Check WIN.INI access and retry uninstall.");saveSetupLog();
   if(!setupSilent)MessageBox(NULL,setupLog,"Uninstall 7-Zip",MB_OK|MB_ICONEXCLAMATION);return 2;
  }
  if(!progmanShortcuts(current,setupDirectory,1)){
   setupLogLine(progmanError);setupLogLine("WARNING: Some Program Manager shortcuts were preserved.");
   if(!setupSilent)MessageBox(NULL,setupLog,"Uninstall 7-Zip",MB_OK|MB_ICONEXCLAMATION);
  }
  sprintf(launch,"%s\\UNINSTC.EXE %s /S",setupDirectory,setupDirectory);
  /* Program Manager's DDE conversation pumps messages. Check again in case
     the File Manager was opened while shortcut removal yielded execution. */
  if(installedManagerRunning())return 2;
  /* Standard/real-mode Windows cannot execute a minimized DOS task. */
  setupResult=WinExec(launch,SW_SHOWNORMAL)>=32?0:2;
  setupLogLine(setupResult?"ERROR: Could not start the removal helper.":"Started file removal helper.");saveSetupLog();return setupResult;
 }
#else
 if(arguments[0]){
  if(strnicmp(arguments,"/S ",3))return 7;
  if(strlen(arguments+3)>=sizeof(setupDirectory))return 7;
  setupSilent=1;strcpy(setupDirectory,arguments+3);
 }
 proc=MakeProcInstance((FARPROC)SetupProc,current);if(!proc)return 2;
 DialogBox(current,MAKEINTRESOURCE(500),NULL,(DLGPROC)proc);FreeProcInstance(proc);return setupResult;
#endif
}
