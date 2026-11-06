/* Drive the unmodified installer Cancel command after its output directory
   exists. Run only in a disposable guest with C:\S33CANC absent. */
#include <windows.h>
#include <stdio.h>
#include <sys/stat.h>
#undef NULL
#define NULL 0
int PASCAL WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show){
 DWORD start;HWND dialog;struct stat info;int sent=0,ok=0;FILE *log;
 (void)instance;(void)previous;(void)command;(void)show;
 log=fopen("C:\\CANC33.TXT","w");if(!log)return 2;
 if(!stat("C:\\S33CANC",&info)){fputs("FAIL destination already exists\n",log);fclose(log);return 2;}
 if(WinExec("C:\\7ZSETW16.EXE /S C:\\S33CANC",SW_SHOWNORMAL)<32){fclose(log);return 2;}
 start=GetTickCount();
 while((DWORD)(GetTickCount()-start)<30000UL){
  dialog=FindWindow(NULL,"7-Zip 26.05 Setup");
  if(!sent&&dialog&&!stat("C:\\S33CANC",&info)){
   PostMessage(dialog,WM_COMMAND,IDCANCEL,0);sent=1;
  }
  if(sent&&!dialog&&stat("C:\\S33CANC",&info)){ok=1;break;}
  Yield();
 }
 fprintf(log,"%s cancellation after directory creation; command sent=%d; output removed=%d\n",ok?"PASS":"FAIL",sent,stat("C:\\S33CANC",&info)!=0);
 fclose(log);return ok?0:2;
}
