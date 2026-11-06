/* Actual GUI -> native WinFile folder, tree and background drop regression.
   Disposable NT guests only; replaces WinFile's saved test window layout. */
#define Z7_WINFILE_SUITE_ENTRY previous_winfile_suite_entry
#include "winfile_drag_suite.c"
#ifndef Z7_WF_FIRST_CASE
#define Z7_WF_FIRST_CASE 0
#endif
static HWND directoryList,treeList;
static POINT destinationPoint;
static BOOL CALLBACK targetLists(HWND window,LPARAM unused)
{
 char name[40];(void)unused;GetClassNameA(window,name,sizeof(name));
 if(!lstrcmpiA(name,"WFS_Dir"))directoryList=GetDlgItem(window,6);
 if(!lstrcmpiA(name,"DirTree"))treeList=GetDlgItem(window,3);
 return TRUE;
}
static BOOL CALLBACK closeSource(HWND window,LPARAM unused)
{
 char name[40];(void)unused;GetClassNameA(window,name,sizeof(name));
 if(!lstrcmpA(name,"7-Zip::FM"))PostMessageA(window,WM_CLOSE,0,0);
 return TRUE;
}
static DWORD WINAPI targetDrag(void *unused)
{
 POINT from={70,32};(void)unused;ClientToScreen(sourceList,&from);
 SetForegroundWindow(sourceWindow);Sleep(100);
 SendMessageA(sourceWindow,WM_COMMAND,600,0);Sleep(100);
 require(SendMessageA(sourceList,LVM_GETSELECTEDCOUNT,0,0)==2,"two source items selected");
 SetCursorPos(from.x,from.y);Sleep(200);mouse_event(MOUSEEVENTF_LEFTDOWN,0,0,0,0);Sleep(200);
 SetCursorPos(from.x+20,from.y+8);Sleep(800);
 if(dragCase==6){keybd_event(VK_CONTROL,0,0,0);keybd_event(VK_SHIFT,0,0,0);}
 else if(dragCase==1||dragCase==3)keybd_event(VK_SHIFT,0,0,0);
 else if(dragCase!=4)keybd_event(VK_CONTROL,0,0,0);
 SetCursorPos(destinationPoint.x,destinationPoint.y);Sleep(1200);
 if(dragCase==4){keybd_event(VK_ESCAPE,0,0,0);Sleep(100);keybd_event(VK_ESCAPE,0,KEYEVENTF_KEYUP,0);Sleep(200);}
 mouse_event(MOUSEEVENTF_LEFTUP,0,0,0,0);Sleep(300);
 keybd_event(VK_CONTROL,0,KEYEVENTF_KEYUP,0);keybd_event(VK_SHIFT,0,KEYEVENTF_KEYUP,0);
 return 0;
}
static void pointAtItem(HWND list,int index)
{
 RECT item;require(SendMessageA(list,LB_GETITEMRECT,index,(LPARAM)&item)!=LB_ERR,"target item rectangle");
 destinationPoint.x=(item.left+item.right)/2;destinationPoint.y=(item.top+item.bottom)/2;
 ClientToScreen(list,&destinationPoint);
 require(WindowFromPoint(destinationPoint)==list,"target item is visible and unobstructed");
}
void entry(void)
{
 char base[MAX_PATH],destDir[MAX_PATH],folder[MAX_PATH],path[MAX_PATH],profile[MAX_PATH],ini[MAX_PATH];
 char install[MAX_PATH],fm[MAX_PATH],cli[MAX_PATH],system[MAX_PATH],dll[MAX_PATH],args[4*MAX_PATH];
 char src[MAX_PATH],input[2][MAX_PATH],dest[2][MAX_PATH],arc[MAX_PATH],saved[MAX_PATH],name[24],treeTitle[MAX_PATH];
 DWORD pid,begin,id,n,code,tid,test,i;HANDLE process,thread,file;MSG message;int treeIndex,treeTop,listTop;RECT rect;
 logFile=CreateFileA("C:\\WFT58.TXT",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 if(logFile==INVALID_HANDLE_VALUE)ExitProcess(2);
 {HWND manager=FindWindowA(0,"Windows NT Task Manager");if(manager)ShowWindow(manager,SW_MINIMIZE);}
 EnumWindows(closeSource,0);
 receiver=FindWindowA("WFS_Frame",0);
 if(receiver){GetWindowThreadProcessId(receiver,&pid);process=OpenProcess(SYNCHRONIZE,FALSE,pid);PostMessageA(receiver,WM_CLOSE,0,0);require(process&&WaitForSingleObject(process,30000)==WAIT_OBJECT_0,"close previous WinFile before DLL replacement");CloseHandle(process);}
 lstrcpyA(install,(GetVersion()&255)==3?"C:\\7-Zip":"C:\\Program Files\\7-Zip");
 wsprintfA(dll,"%s\\7zWF.dll",install);require(CopyFileA("A:\\7zWF.dll",dll,FALSE),"install exact test WinFile add-on");
 wsprintfA(fm,"%s\\7zFM.exe",install);wsprintfA(cli,"%s\\7z.exe",install);
 id=GetTickCount()&0xfffff;wsprintfA(base,"C:\\H58%05lX",id);require(CreateDirectoryA(base,0),"private regression directory");record(base);
 wsprintfA(destDir,"%s\\DEST",base);require(CreateDirectoryA(destDir,0),"private destination directory");
 for(i=0;i<3;i++){wsprintfA(folder,"%s\\%s",destDir,i==0?"A-LIST":i==1?"B-TREE":"C-KEEP");require(CreateDirectoryA(folder,0),"private target or selection folder");}
 GetWindowsDirectoryA(ini,MAX_PATH);lstrcatA(ini,"\\WINFILE.INI");
 wsprintfA(profile,"0,0,380,260,-1,-1,3,0,201,2033,150,%s\\*.*",destDir);
 require(WritePrivateProfileStringA("Settings","dir1",profile,ini)&&WritePrivateProfileStringA("Settings","dir2",NULL,ini),"prepare single private WinFile destination window");
 require(WritePrivateProfileStringA("AddOns","7-Zip",dll,ini),"register current add-on");
 GetSystemDirectoryA(system,MAX_PATH);lstrcatA(system,"\\WINFILE.EXE");process=start(system,"",0);
 begin=GetTickCount();receiver=0;while(!receiver&&GetTickCount()-begin<60000){receiver=FindWindowA("WFS_Frame",0);Sleep(50);}
 require(receiver!=0,"real WinFile opens");WaitForInputIdle(process,30000);CloseHandle(process);Sleep(1500);
 ShowWindow(receiver,SW_RESTORE);SetWindowPos(receiver,HWND_TOP,230,30,410,440,SWP_SHOWWINDOW);
 EnumChildWindows(receiver,targetLists,0);require(directoryList&&treeList,"actual WinFile directory and tree lists");
 begin=GetTickCount();while(SendMessageA(directoryList,LB_GETCOUNT,0,0)!=4&&GetTickCount()-begin<30000)Sleep(50);
 require(SendMessageA(directoryList,LB_GETCOUNT,0,0)==4,"parent plus three private folders visible");
 treeIndex=(int)SendMessageA(treeList,LB_GETCURSEL,0,0);require(treeIndex>=0,"current destination selected in tree");
 SendMessageA(GetParent(treeList),0x946,FALSE,0); /* expand current node */
 SendMessageA(treeList,LB_SETTOPINDEX,treeIndex>0?treeIndex-1:0,0);
 require(SendMessageA(treeList,LB_GETCOUNT,0,0)>treeIndex+2,"tree contains both target child folders");
 for(test=Z7_WF_FIRST_CASE;test<9;test++){
  wsprintfA(args,"CASE %lu: %s",test,test==0?"filesystem copy to unselected list folder":test==1?"filesystem move to unselected tree folder":test==2?"archive copy to list folder":test==3?"archive optimized move to tree folder":test==4?"cancel on list folder":test==5?"copy to directory background":test==6?"unsupported link does not copy or move":test==7?"toolbar is not a drop destination":"copy to parent directory row");record(args);
  wsprintfA(src,"%s\\SRC%lu",base,test);require(CreateDirectoryA(src,0),"new source directory");
  for(i=0;i<2;i++){
   wsprintfA(name,"T58%lu%lu.TXT",test,i);wsprintfA(input[i],"%s\\%s",src,name);
   wsprintfA(dest[i],"%s\\%s%s",test==8?base:destDir,test==5||test>=7?"":test==1||test==3?"B-TREE\\":"A-LIST\\",name);record(dest[i]);
   file=CreateFileA(input[i],GENERIC_WRITE,0,0,CREATE_NEW,0,0);require(file!=INVALID_HANDLE_VALUE&&WriteFile(file,payload,sizeof(payload)-1,&n,0)&&n==sizeof(payload)-1,"write exact source bytes");CloseHandle(file);
  }
  if(test==2||test==3){
   wsprintfA(arc,"%s\\ARC%lu.7Z",base,test);wsprintfA(saved,"%s\\SAVE%lu.7Z",base,test);wsprintfA(args,"a -t7z -mx1 \"%s\" \"%s\\*\"",arc,src);
   process=start(cli,args,0);require(WaitForSingleObject(process,90000)==WAIT_OBJECT_0&&GetExitCodeProcess(process,&code)&&!code,"create two-file archive");CloseHandle(process);require(CopyFileA(arc,saved,TRUE),"save original archive bytes");
  }
  sourceWindow=sourceList=0;wsprintfA(args,"\"%s\"",test==2||test==3?arc:src);process=start(fm,args,&sourcePid);
  begin=GetTickCount();while(!sourceWindow&&GetTickCount()-begin<90000){EnumWindows(findMain,0);Sleep(30);}
  require(sourceWindow!=0,"actual 7-Zip source visible");WaitForInputIdle(process,30000);
  SetWindowPos(sourceWindow,HWND_TOP,0,0,220,460,SWP_SHOWWINDOW);EnumChildWindows(sourceWindow,findList,0);require(sourceList!=0,"actual source list");
  begin=GetTickCount();while(SendMessageA(sourceList,LVM_GETITEMCOUNT,0,0)!=2&&GetTickCount()-begin<15000)Sleep(30);
  require(SendMessageA(sourceList,LVM_GETITEMCOUNT,0,0)==2,"two source files");
  SetWindowPos(receiver,HWND_TOP,230,30,410,440,SWP_SHOWWINDOW);
  treeIndex=(int)SendMessageA(treeList,LB_GETCURSEL,0,0);
  GetWindowTextA(GetParent(GetParent(treeList)),treeTitle,sizeof(treeTitle));
  SendMessageA(directoryList,LB_SETSEL,FALSE,-1);SendMessageA(directoryList,LB_SETSEL,TRUE,2);SendMessageA(directoryList,LB_SETSEL,TRUE,3);SendMessageA(directoryList,LB_SETCARETINDEX,3,FALSE);
  SendMessageA(GetParent(directoryList),WM_COMMAND,MAKEWPARAM(6,LBN_SELCHANGE),(LPARAM)directoryList);
  listTop=(int)SendMessageA(directoryList,LB_GETTOPINDEX,0,0);treeTop=(int)SendMessageA(treeList,LB_GETTOPINDEX,0,0);
  if(test==8)pointAtItem(directoryList,0);
  else if(test==1||test==3)pointAtItem(treeList,treeIndex+2);
  else if(test<5||test==6)pointAtItem(directoryList,1);
  else if(test==7){destinationPoint.x=20;destinationPoint.y=10;ClientToScreen(receiver,&destinationPoint);require(IsChild(receiver,WindowFromPoint(destinationPoint)),"non-directory frame area is visible");}
  else{GetClientRect(directoryList,&rect);destinationPoint.x=rect.right-20;destinationPoint.y=rect.bottom-20;ClientToScreen(directoryList,&destinationPoint);require(WindowFromPoint(destinationPoint)==directoryList,"background is visible");}
  dragCase=test;thread=CreateThread(0,0,targetDrag,0,0,&tid);require(thread!=0,"start real multi-file drag");begin=GetTickCount();
  while(GetTickCount()-begin<45000){
   while(PeekMessageA(&message,0,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageA(&message);}
   if(WaitForSingleObject(thread,0)==WAIT_OBJECT_0&&(test==4||test==6||test==7||(GetFileAttributesA(dest[0])!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesA(dest[1])!=INVALID_FILE_ATTRIBUTES)))break;
   Sleep(30);
  }
  require(WaitForSingleObject(thread,1000)==WAIT_OBJECT_0,"mouse driver completes");CloseHandle(thread);Sleep(400);
  for(i=0;i<2;i++){
   if(test==4||test==6||test==7)require(GetFileAttributesA(dest[i])==INVALID_FILE_ATTRIBUTES,"cancelled or unsupported drop creates no target file");else exact(dest[i]);
   if(test==1)require(GetFileAttributesA(input[i])==INVALID_FILE_ATTRIBUTES,"physical move removes source once");else exact(input[i]);
   if(test!=5){wsprintfA(path,"%s\\T58%lu%lu.TXT",destDir,test,i);require(GetFileAttributesA(path)==INVALID_FILE_ATTRIBUTES,"folder drop does not leak into current directory");}
  }
  require(SendMessageA(directoryList,LB_GETSELCOUNT,0,0)==2&&SendMessageA(directoryList,LB_GETSEL,2,0)>0&&SendMessageA(directoryList,LB_GETSEL,3,0)>0,"existing directory multiselection preserved");
  require(SendMessageA(directoryList,LB_GETCARETINDEX,0,0)==3&&SendMessageA(directoryList,LB_GETTOPINDEX,0,0)==listTop,"directory caret and scroll preserved");
  GetWindowTextA(GetParent(GetParent(treeList)),path,sizeof(path));
  wsprintfA(args,"Tree selection/top before %d/%d after %ld/%ld title equal=%d",treeIndex,treeTop,SendMessageA(treeList,LB_GETCURSEL,0,0),SendMessageA(treeList,LB_GETTOPINDEX,0,0),!lstrcmpA(path,treeTitle));record(args);
  require(!lstrcmpA(path,treeTitle)&&SendMessageA(treeList,LB_GETCURSEL,0,0)-SendMessageA(treeList,LB_GETTOPINDEX,0,0)==treeIndex-treeTop,"current tree directory and its visible row preserved after refresh");
  PostMessageA(sourceWindow,WM_CLOSE,0,0);require(WaitForSingleObject(process,15000)==WAIT_OBJECT_0&&GetExitCodeProcess(process,&code)&&!code,"GUI closes normally");CloseHandle(process);
  if(test==2||test==3)require(sameFile(arc,saved),"original archive bytes preserved");
 }
 record("PASS ALL WINFILE TARGETS");CloseHandle(logFile);ExitProcess(0);
}
