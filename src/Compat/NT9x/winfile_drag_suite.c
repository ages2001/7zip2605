/* Real 7-Zip -> WinFile multi-file copy/move/cancel regression.
   Disposable NT guests only. WinFile must show C:\ in its directory pane. */
#define entry outbound_suite_entry
#include "outbound_drop_tests.c"
#undef entry
static unsigned dragCase;
static DWORD WINAPI driveWinFile(void *unused)
{
 POINT from={70,32},to={370,150};
 (void)unused;
 ClientToScreen(sourceList,&from);ClientToScreen(receiver,&to);
 SetForegroundWindow(sourceWindow);Sleep(100);
 SendMessageA(sourceWindow,WM_COMMAND,600,0); /* Select All */
 Sleep(100);
 require(SendMessageA(sourceList,LVM_GETSELECTEDCOUNT,0,0)==2,"both source items selected");
 SetCursorPos(from.x,from.y);Sleep(200);
 mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);Sleep(200);
 SetCursorPos(from.x+20,from.y+8);Sleep(800);
 if(dragCase==1||dragCase==4)keybd_event(VK_SHIFT,0,0,0);
 else if(dragCase!=2)keybd_event(VK_CONTROL,0,0,0);
 SetCursorPos(to.x,to.y);Sleep(1200);
 if(dragCase==2){keybd_event(VK_ESCAPE,0,0,0);Sleep(100);keybd_event(VK_ESCAPE,0,KEYEVENTF_KEYUP,0);Sleep(200);}
 mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);Sleep(300);
 keybd_event(VK_CONTROL,0,KEYEVENTF_KEYUP,0);
 keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);return 0;
}
static BOOL sameFile(const char *left,const char *right)
{
 HANDLE a,b;BYTE x[4096],y[4096];DWORD nx,ny,i;BOOL equal=TRUE;
 a=CreateFileA(left,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
 b=CreateFileA(right,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
 if(a==INVALID_HANDLE_VALUE||b==INVALID_HANDLE_VALUE)equal=FALSE;
 else for(;;){
  if(!ReadFile(a,x,sizeof(x),&nx,0)||!ReadFile(b,y,sizeof(y),&ny,0)||nx!=ny){equal=FALSE;break;}
  for(i=0;i<nx;i++)if(x[i]!=y[i]){equal=FALSE;break;}
  if(!equal||!nx)break;
 }
 if(a!=INVALID_HANDLE_VALUE)CloseHandle(a);if(b!=INVALID_HANDLE_VALUE)CloseHandle(b);return equal;
}
#ifndef Z7_WINFILE_SUITE_ENTRY
#define Z7_WINFILE_SUITE_ENTRY entry
#endif
void Z7_WINFILE_SUITE_ENTRY(void)
{
 char base[MAX_PATH],src[MAX_PATH],input[2][MAX_PATH],dest[2][MAX_PATH];
 char arc[MAX_PATH],saved[MAX_PATH],fm[MAX_PATH],cli[MAX_PATH],args[4*MAX_PATH],name[20];
 DWORD id,n,code,begin,tid,test,i;HANDLE file,process,thread;MSG message;
 logFile=CreateFileA("C:\\WFD57.TXT",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 {HWND manager=FindWindowA(0,"Windows NT Task Manager");if(manager)ShowWindow(manager,SW_MINIMIZE);}
 receiver=FindWindowA(0,"File Manager");require(receiver!=0,"actual Windows File Manager is open");
 id=GetTickCount()&0xfffff;wsprintfA(base,"C:\\W57%05lX",id);
 require(CreateDirectoryA(base,0),"private regression directory");record(base);
 lstrcpyA(fm,(GetVersion()&255)==3?"C:\\7-Zip":"C:\\Program Files\\7-Zip");
 wsprintfA(cli,"%s\\7z.exe",fm);lstrcatA(fm,"\\7zFM.exe");
 for(test=0;test<5;test++){
  wsprintfA(args,"CASE %lu: %s",test,test==0?"filesystem copy":test==1?"filesystem move":test==2?"Escape cancellation":test==3?"archive copy":"archive optimized move");record(args);
  wsprintfA(src,"%s\\SRC%lu",base,test);require(CreateDirectoryA(src,0),"create source directory");
  for(i=0;i<2;i++){
   wsprintfA(name,"W%05lX%lu.TXT",(id+test)&0xfffff,i);
   wsprintfA(input[i],"%s\\%s",src,name);wsprintfA(dest[i],"C:\\%s",name);
   require(GetFileAttributesA(dest[i])==INVALID_FILE_ATTRIBUTES,"destination absent; never overwrite unrelated files");
   record(dest[i]);file=CreateFileA(input[i],GENERIC_WRITE,0,0,CREATE_NEW,0,0);
   require(file!=INVALID_HANDLE_VALUE&&WriteFile(file,payload,sizeof(payload)-1,&n,0)&&n==sizeof(payload)-1,"write exact source fixture");CloseHandle(file);
  }
  if(test>=3){
   wsprintfA(arc,"%s\\ARC%lu.7Z",base,test);wsprintfA(saved,"%s\\SAVE%lu.7Z",base,test);
   wsprintfA(args,"a -t7z -mx1 \"%s\" \"%s\\*\"",arc,src);
   process=start(cli,args,0);require(WaitForSingleObject(process,90000)==WAIT_OBJECT_0&&GetExitCodeProcess(process,&code)&&!code,"create two-file archive");CloseHandle(process);
   require(CopyFileA(arc,saved,TRUE),"preserve original archive for byte comparison");
  }
  sourceWindow=sourceList=0;wsprintfA(args,"\"%s\"",test>=3?arc:src);process=start(fm,args,&sourcePid);
  begin=GetTickCount();while(!sourceWindow&&GetTickCount()-begin<90000){EnumWindows(findMain,0);Sleep(30);}
  require(sourceWindow!=0,"actual 7-Zip source window");WaitForInputIdle(process,30000);
  SetWindowPos(sourceWindow,HWND_TOP,0,0,220,460,SWP_SHOWWINDOW);EnumChildWindows(sourceWindow,findList,0);
  require(sourceList!=0,"source list available");begin=GetTickCount();
  while(SendMessageA(sourceList,LVM_GETITEMCOUNT,0,0)!=2&&GetTickCount()-begin<15000)Sleep(30);
  require(SendMessageA(sourceList,LVM_GETITEMCOUNT,0,0)==2,"two source items");
  ShowWindow(receiver,SW_RESTORE);SetWindowPos(receiver,HWND_TOP,230,60,410,410,SWP_SHOWWINDOW);
  dragCase=test;thread=CreateThread(0,0,driveWinFile,0,0,&tid);require(thread!=0,"start actual mouse drag");
  begin=GetTickCount();
  while(GetTickCount()-begin<30000){
   while(PeekMessageA(&message,0,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageA(&message);}
   if(WaitForSingleObject(thread,0)==WAIT_OBJECT_0&&(test==2||(GetFileAttributesA(dest[0])!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesA(dest[1])!=INVALID_FILE_ATTRIBUTES)))break;
   Sleep(30);
  }
  require(WaitForSingleObject(thread,1000)==WAIT_OBJECT_0,"mouse driver completes");CloseHandle(thread);Sleep(300);
  for(i=0;i<2;i++){
   if(test==2)require(GetFileAttributesA(dest[i])==INVALID_FILE_ATTRIBUTES,"Escape creates no destination file");else exact(dest[i]);
   if(test==1)require(GetFileAttributesA(input[i])==INVALID_FILE_ATTRIBUTES,"physical move removes source once");else exact(input[i]);
  }
  PostMessageA(sourceWindow,WM_CLOSE,0,0);require(WaitForSingleObject(process,10000)==WAIT_OBJECT_0&&GetExitCodeProcess(process,&code)&&!code,"7-Zip closes normally");CloseHandle(process);
  if(test>=3)require(sameFile(arc,saved),"source archive remains byte-for-byte unchanged");
 }
 record("PASS ALL WINFILE DRAGS");CloseHandle(logFile);ExitProcess(0);
}
