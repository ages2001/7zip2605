/* Native Windows 3.0 archive browser; 16-bit front end.
   Shared parser/test logic is also used by the DOS command frontend. */
#define Z7_GUI_FRONTEND
#include "console.c"
#include <windows.h>
#include "source_list.h"
#include "associations.h"
/* Owner drawing shares the resident GUI callback segment. */
#include "toolbar.c"
#include "../../CPP/7zip/UI/FileManager/OriginalLogo.h"
#define ID_OPEN 101
#define ID_TEST 102
#define ID_CLOSE 103
#define ID_EXIT 104
#define ID_ABOUT 105
#define ID_CANCEL 106
#define ID_EXTRACT 107
#define ID_CREATE 108
#define ID_RENAME 109
#define ID_UPDATE 113
#define ID_DELETE 114
#define ID_ASSOC 116
#define ID_HASH 117
#define ID_SELFTEST (WM_USER+1)
static HINSTANCE instance;
static HWND mainWindow,pathEdit,fileList,statusText,openButton,testButton,cancelButton,extractButton,createButton;
static HWND copyButton,moveButton,deleteButton,infoButton;
static unsigned toolbarPaintFailures;
static SRes lastResult;
static BOOL uiOnlyTest,uiUpdateTest,deflate64OnlyTest;
static Input input;
static CLookToRead2 look;
static CSzArEx archive;
/* Locked Windows global blocks remain stable in every Windows 3.x mode.
   Keep the owning handle with the block instead of using the C runtime far heap. */
static unsigned windowAllocationCount;
/* Controlled by the integration driver only; normal operations leave these
   zero. Exercise real-mode recovery and persistent exhaustion in the guest. */
static unsigned windowAllocationFailuresRemaining,windowAllocationFailures;
static unsigned windowAllocationFailureSize;
static BOOL windowAllocationCancelOnFailure;
static BOOL aboutTesting,aboutLogoDrawn;
BOOL FAR PASCAL __export AboutProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 (void)lParam;
 switch(message){
 case WM_INITDIALOG:aboutLogoDrawn=FALSE;if(aboutTesting)SetTimer(window,1,500,NULL);return TRUE;
 case WM_PAINT:{PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);aboutLogoDrawn=DrawOriginalLogo(instance,100,dc,12,12);EndPaint(window,&paint);return TRUE;}
 case WM_TIMER:if(aboutTesting){HDC dc=GetDC(window);char text[80];BOOL valid=aboutLogoDrawn;
  valid=valid&&GetPixel(dc,13,13)==RGB(0,0,0)&&GetPixel(dc,12,12)==GetPixel(dc,11,12);
  ReleaseDC(window,dc);GetDlgItemText(window,201,text,sizeof(text));valid=valid&&!strcmp(text,"7-Zip 26.05 (x86-16)");
  KillTimer(window,1);EndDialog(window,valid?1:0);return TRUE;}break;
 case WM_COMMAND:if(wParam==IDOK||wParam==IDCANCEL){EndDialog(window,0);return TRUE;}break;
 case WM_CLOSE:EndDialog(window,0);return TRUE;
 }
 return FALSE;
}
static int showAbout(HWND parent){
 int result=-1;FARPROC proc=MakeProcInstance((FARPROC)AboutProc,instance);
 if(proc){result=DialogBox(instance,MAKEINTRESOURCE(200),parent,(DLGPROC)proc);FreeProcInstance(proc);}return result;
}
static char associationExecutable[260];
static BOOL associationTesting,associationOnlyTest;
static unsigned associationChoices;
BOOL FAR PASCAL __export AssociationsProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 (void)lParam;
 switch(message){
 case WM_INITDIALOG:
  CheckDlgButton(window,601,assocCurrent(associationExecutable,"7z"));CheckDlgButton(window,602,assocCurrent(associationExecutable,"zip"));
  if(associationTesting){CheckDlgButton(window,601,associationChoices&1);CheckDlgButton(window,602,!!(associationChoices&2));PostMessage(window,WM_COMMAND,IDOK,0);}return TRUE;
 case WM_COMMAND:
  if(wParam==IDOK){int a=assocSet(associationExecutable,"7z",IsDlgButtonChecked(window,601)),b=assocSet(associationExecutable,"zip",IsDlgButtonChecked(window,602));
   if(!a||!b){if(!associationTesting)MessageBox(window,"Some associations could not be changed. Check WIN.INI access and whether another 7-Zip installation owns the association. Existing restoration records were preserved where needed.","File associations",MB_OK|MB_ICONEXCLAMATION);EndDialog(window,0);}
   else EndDialog(window,1);return TRUE;
  }
  if(wParam==IDCANCEL){EndDialog(window,0);return TRUE;}break;
 case WM_CLOSE:EndDialog(window,0);return TRUE;
 }
 return FALSE;
}
static int showAssociations(HWND parent){FARPROC proc=MakeProcInstance((FARPROC)AssociationsProc,instance);int result=0;if(proc){result=DialogBox(instance,MAKEINTRESOURCE(600),parent,(DLGPROC)proc);FreeProcInstance(proc);}return result;}
static int openArgument(LPSTR command,char *path){
 unsigned n=0;int quoted=0;
 while(*command==' '||*command=='\t')command++;
 if(!*command)return 0;if(*command=='"'){quoted=1;command++;}
 while(*command&&(quoted?*command!='"':*command!=' '&&*command!='\t')){if(n>=259)return -1;path[n++]=*command++;}
 if(quoted){if(*command!='"')return -1;command++;}
 while(*command==' '||*command=='\t')command++;path[n]=0;
 return *command||!n||(!validDosDestination(path)&&!(n==3&&path[1]==':'&&(path[2]=='\\'||path[2]=='/')&&foldName((unsigned char)path[0])>='A'&&foldName((unsigned char)path[0])<='Z'))?-1:1;
}
static BOOL hashTesting,hashCancelAfterRead,hashOnlyTest,hashAfterAssociation;
static BOOL dictionaryOnlyTest,filterOnlyTest,updateOnlyTest,browseOnlyTest,bcjOnlyTest,bcjUpdateTest;
static UInt32 dictionaryCancelAfter;
static unsigned hashChoice=4;
static char hashSource[260],hashResults[1024];
static char extractDestination[260]="C:\\7ZOUT";
static BOOL extractTesting,extractFlat,extractCancelTesting;
static BOOL flatOnlyTest,treeOnlyTest,selectionOnlyTest;
BOOL FAR PASCAL __export ExtractProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 (void)lParam;
 switch(message){
 case WM_INITDIALOG:SetWindowText(window,SendMessage(fileList,LB_GETSELCOUNT,0,0)>0?"Extract selected items":"Extract entire archive");CheckDlgButton(window,302,extractFlat);SetDlgItemText(window,301,extractDestination);SendDlgItemMessage(window,301,EM_LIMITTEXT,250,0);if(extractTesting)PostMessage(window,WM_COMMAND,IDOK,0);return TRUE;
 case WM_COMMAND:
  if(wParam==IDOK){extractFlat=IsDlgButtonChecked(window,302);GetDlgItemText(window,301,extractDestination,sizeof(extractDestination));if(!extractDestination[0]){MessageBox(window,"Enter a new destination directory.","Extract",MB_OK);return TRUE;}EndDialog(window,1);if(extractTesting&&extractCancelTesting)PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);return TRUE;}
  if(wParam==IDCANCEL){EndDialog(window,0);return TRUE;}break;
 case WM_CLOSE:EndDialog(window,0);return TRUE;
 }
 return FALSE;
}
static char renameOld[260],renameNew[260];
static BOOL renameTesting,renameCancelAfterWrite;
BOOL FAR PASCAL __export RenameProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 (void)lParam;
 switch(message){
 case WM_INITDIALOG:SetDlgItemText(window,501,renameOld);SetDlgItemText(window,502,renameNew);SendDlgItemMessage(window,502,EM_LIMITTEXT,250,0);if(renameTesting)PostMessage(window,WM_COMMAND,IDOK,0);return TRUE;
 case WM_COMMAND:if(wParam==IDOK){GetDlgItemText(window,502,renameNew,sizeof(renameNew));if(!renamePath(renameNew)){MessageBox(window,"Enter a relative DOS 8.3 path.","Rename archive entry",MB_OK);return TRUE;}EndDialog(window,1);return TRUE;}if(wParam==IDCANCEL){EndDialog(window,0);return TRUE;}break;
 case WM_CLOSE:EndDialog(window,0);return TRUE;
 }
 return FALSE;
}
#include "create_source.h"
static char createDestination[260]="C:\\NEW.7Z";
static int createMethod=1;
static BOOL createZipMode,zipCreateTesting;
static int createFormat; /* create dialog format: 0=7z 1=zip 2=gz 3=tar 4=xz */
static BOOL createTesting,createCancelTesting,createCancelAfterRead,createUpdating,deleteTesting;
#ifdef __WATCOMC__
#pragma code_seg("CREATE_TEXT")
#endif
static void fillCreateMethods(HWND window){
 SendDlgItemMessage(window,403,CB_RESETCONTENT,0,0);
 if(createFormat>=2){
  /* Single-method stream formats: gzip (Deflate), tar (store), xz (LZMA2). */
  SendDlgItemMessage(window,403,CB_ADDSTRING,0,(LPARAM)(LPSTR)
   (createFormat==2?"Deflate (gzip)":createFormat==3?"Store (tar)":"LZMA2 (xz, 8 KiB)"));
  createMethod=0;SendDlgItemMessage(window,403,CB_SETCURSEL,0,0);
  EnableWindow(GetDlgItem(window,403),FALSE);
#ifdef Z7_WIN16_WRITE_XZ_AES
  EnableWindow(GetDlgItem(window,407),FALSE); /* AES password applies to 7z only */
#endif
  return;
 }
 EnableWindow(GetDlgItem(window,403),TRUE);
#ifdef Z7_WIN16_WRITE_XZ_AES
 EnableWindow(GetDlgItem(window,407),createFormat==0&&!createUpdating);
#endif
 SendDlgItemMessage(window,403,CB_ADDSTRING,0,(LPARAM)(LPSTR)"Copy / Store (no compression)");
 SendDlgItemMessage(window,403,CB_ADDSTRING,0,(LPARAM)(LPSTR)(createZipMode?"Deflate (8 KiB)":"LZMA (8 KiB, fast)"));
 if(createZipMode)SendDlgItemMessage(window,403,CB_ADDSTRING,0,(LPARAM)(LPSTR)"Deflate64 (8 KiB)");
 if(createMethod<0||createMethod>(createZipMode?2:1))createMethod=1;
 SendDlgItemMessage(window,403,CB_SETCURSEL,createMethod,0);
}
BOOL FAR PASCAL __export CreateProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 (void)lParam;
 switch(message){
 case WM_INITDIALOG:
  SetDlgItemText(window,401,createDestination);SetDlgItemText(window,402,createSource?createSource:"");
  if(createUpdating){SetWindowText(window,"Add or replace archive files");SetDlgItemText(window,404,"Existing archive:");SetDlgItemText(window,405,"Matching names are replaced. Other entries are retained. Use DOS 8.3 paths.");SetDlgItemText(window,IDOK,"Update");EnableWindow(GetDlgItem(window,401),FALSE);}
  if(createUpdating)createZipMode=archiveIsZip(createDestination);
  createFormat=createZipMode?1:0;
  SendDlgItemMessage(window,406,CB_ADDSTRING,0,(LPARAM)(LPSTR)"7z");SendDlgItemMessage(window,406,CB_ADDSTRING,0,(LPARAM)(LPSTR)"ZIP");
  SendDlgItemMessage(window,406,CB_ADDSTRING,0,(LPARAM)(LPSTR)"gz (one file)");SendDlgItemMessage(window,406,CB_ADDSTRING,0,(LPARAM)(LPSTR)"tar (store)");
#ifdef Z7_WIN16_WRITE_XZ_AES
  SendDlgItemMessage(window,406,CB_ADDSTRING,0,(LPARAM)(LPSTR)"xz (one file)");
#endif
  SendDlgItemMessage(window,406,CB_SETCURSEL,createFormat,0);EnableWindow(GetDlgItem(window,406),!createUpdating);
#ifndef Z7_WIN16_WRITE_XZ_AES
  /* AES password is a protected-mode write feature; hide the field when the
     build omits it so the control is never shown as an inert input. */
  ShowWindow(GetDlgItem(window,407),SW_HIDE);ShowWindow(GetDlgItem(window,408),SW_HIDE);
#endif
  fillCreateMethods(window);
  SendDlgItemMessage(window,401,EM_LIMITTEXT,250,0);SendDlgItemMessage(window,402,EM_LIMITTEXT,4095,0);
  if(zipCreateTesting){SendDlgItemMessage(window,406,CB_SETCURSEL,1,0);SendMessage(window,WM_COMMAND,406,MAKELONG((WORD)GetDlgItem(window,406),CBN_SELCHANGE));}
  if(createTesting)PostMessage(window,WM_COMMAND,IDOK,0);return TRUE;
 case WM_COMMAND:
  if(wParam==406&&HIWORD(lParam)==CBN_SELCHANGE){
   if(createFormat<2){createMethod=(int)SendDlgItemMessage(window,403,CB_GETCURSEL,0,0);if(createMethod<0)createMethod=1;}
   createFormat=(int)SendDlgItemMessage(window,406,CB_GETCURSEL,0,0);if(createFormat<0)createFormat=0;
   createZipMode=(createFormat==1);
   fillCreateMethods(window);return TRUE;
  }
  if(wParam==IDOK){
   GetDlgItemText(window,401,createDestination,sizeof(createDestination));if(!replaceCreateSource(NULL,GetDlgItem(window,402))){lastResult=SZ_ERROR_MEM;if(createTesting)EndDialog(window,0);else MessageBox(window,"Not enough memory to store the source list. The previous list was preserved.","Create archive",MB_OK|MB_ICONEXCLAMATION);return TRUE;}
   createMethod=(int)SendDlgItemMessage(window,403,CB_GETCURSEL,0,0);if(createMethod<0||createMethod>(createZipMode?2:1))createMethod=1;
#ifdef Z7_WIN16_WRITE_XZ_AES
   {char pwd[128];GetDlgItemText(window,407,pwd,sizeof(pwd));
    if(pwd[0]&&createFormat==0&&!createUpdating){aes7zEncSetPassword(pwd);createMethod=1;/* AES-256 needs the LZMA coder (7z only) */}
    else aes7zEncSetPassword((const char *)0);}
#endif
   if(!createDestination[0]||!createSource[0]){MessageBox(window,"Enter the archive path and one source file or folder path per line.","Create archive",MB_OK);return TRUE;}
   EndDialog(window,1);return TRUE;
  }
  if(wParam==IDCANCEL){EndDialog(window,0);return TRUE;}break;
 case WM_CLOSE:EndDialog(window,0);return TRUE;
 }
 return FALSE;
}
#ifdef __WATCOMC__
#pragma code_seg()
#endif
static void *windowAllocate(ISzAllocPtr unused,size_t size){
 HGLOBAL handle;Byte FAR *block;(void)unused;
 if(size==(windowAllocationFailureSize?windowAllocationFailureSize:4096)&&windowAllocationFailuresRemaining){
  windowAllocationFailuresRemaining--;windowAllocationFailures++;
  if(windowAllocationCancelOnFailure){windowAllocationCancelOnFailure=FALSE;PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);}
  return NULL;
 }
 if(size>(size_t)-1-8)return NULL;
 handle=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,(DWORD)size+8);
 /* Real-mode Windows can leave discardable code occupying otherwise usable
    conventional memory after a nested dialog or another application exits.
    Locked archive blocks cannot move. Compact once before reporting failure;
    protected modes retain their existing allocation path. */
 if(!handle&&!(GetWinFlags()&WF_PMODE)){
  GlobalCompact(0xFFFFFFFFUL);
  handle=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,(DWORD)size+8);
 }
 if(!handle){
#ifdef Z7_ALLOC_FAILURE_TRACE
  FILE *trace=fopen("C:\\MEM66.TXT","a");if(trace){
   setvbuf(trace,NULL,_IONBF,0);fprintf(trace,"ALLOC FAILED bytes=%u free=%lu largest=%lu blocks=%u\n",size,GetFreeSpace(0),GlobalCompact(0),windowAllocationCount);fclose(trace);
  }
#endif
  return NULL;
 }
 block=(Byte FAR *)GlobalLock(handle);
 if(!block){GlobalFree(handle);return NULL;}
 *(HGLOBAL FAR *)block=handle;windowAllocationCount++;return block+8;
}
static void windowRelease(ISzAllocPtr unused,void *pointer){
 Byte FAR *block;HGLOBAL handle;(void)unused;if(!pointer)return;
 block=(Byte FAR *)pointer-8;handle=*(HGLOBAL FAR *)block;
 GlobalUnlock(handle);if(!GlobalFree(handle))windowAllocationCount--;
}
#ifdef Z7_GUI_TRACE
static void trace(const char *text){FILE *file=fopen("C:\\G16TRACE.TXT","a");if(file){fputs(text,file);fputc('\n',file);fclose(file);}}
static unsigned dataSegment(void);
#pragma aux dataSegment = "mov ax,ds" value [ax] modify exact [ax];
static unsigned stackSegment(void);
#pragma aux stackSegment = "mov ax,ss" value [ax] modify exact [ax];
static unsigned cpuFlags(void);
#pragma aux cpuFlags = "pushf" "pop ax" value [ax] modify exact [ax];
static void traceRegisters(void){static char text[80];sprintf(text,"DS=%04X SS=%04X FLAGS=%04X",dataSegment(),stackSegment(),cpuFlags());trace(text);}
static void *guiAllocate(ISzAllocPtr p,size_t n){void *v;char line[100];sprintf(line,"allocate %u",n);trace(line);v=windowAllocate(p,n);sprintf(line,"allocated %p",v);trace(line);return v;}
static ISzAlloc allocator={guiAllocate,windowRelease};
#else
#define trace(text) ((void)0)
#define traceRegisters() ((void)0)
static ISzAlloc allocator={windowAllocate,windowRelease};
#endif
/* Streaming readers accept short look buffers. Keep conventional memory
   available for USER/GDI during nested real-mode drawing callbacks. */
static Byte realLookBuffer[512];
static Byte *lookBuffer=realLookBuffer;
static unsigned lookCapacity=sizeof(realLookBuffer);
static HGLOBAL lookStorage;
static BOOL initLookBuffer(void){
 if(!(GetWinFlags()&(WF_STANDARD|WF_ENHANCED)))return TRUE;
 lookStorage=GlobalAlloc(GMEM_MOVEABLE,4096);if(!lookStorage)return FALSE;
 lookBuffer=(Byte *)GlobalLock(lookStorage);
 if(!lookBuffer){GlobalFree(lookStorage);lookStorage=0;lookBuffer=realLookBuffer;return FALSE;}
 lookCapacity=4096;return TRUE;
}
static void freeLookBuffer(void){
 if(lookStorage){GlobalUnlock(lookStorage);GlobalFree(lookStorage);}
 lookStorage=0;lookBuffer=realLookBuffer;lookCapacity=sizeof(realLookBuffer);
}
static int opened,busy,cancelled,closePending,selfTest;
static BOOL folderView;
static BOOL fileListMultiple;
static long selectionAnchor=-1;
/* Selection styles are fixed when a list box is created. Recreate the empty
   control when changing between the filesystem and archive views instead of
   changing LBS_EXTENDEDSEL on an existing Windows 3.0 control. */
/* Report-view column layout (pixels from the list client left), header-bar
   geometry and item kinds, shared by the owner-drawn list and its header. */
#define Z7_COL_NAME    22
#define Z7_COL_SIZE_L  250
#define Z7_COL_SIZE_R  344
#define Z7_COL_DATE    356
#define Z7_COL_ATTR    496
#define Z7_COL_END     560
#define Z7_HDR_TOP     96
#define Z7_HDR_H       18
#define Z7_LIST_TOP    (Z7_HDR_TOP+Z7_HDR_H)
#define Z7_ITEM_FILE   0
#define Z7_ITEM_DIR    1
#define Z7_ITEM_UP     2
static HWND listHeader;
static BOOL configureFileList(BOOL multiple){
 HWND replacement,old=fileList;RECT rect;BOOL focused=old&&GetFocus()==old;
 if(old&&multiple==fileListMultiple)return TRUE;
 GetClientRect(mainWindow,&rect);
 replacement=CreateWindow("LISTBOX",NULL,WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|WS_HSCROLL|WS_TABSTOP|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|LBS_NOINTEGRALHEIGHT|(multiple?LBS_EXTENDEDSEL:0),8,Z7_LIST_TOP,rect.right>16?rect.right-16:1,rect.bottom>(Z7_LIST_TOP+44)?rect.bottom-(Z7_LIST_TOP+44):1,mainWindow,(HMENU)111,instance,NULL);
 if(!replacement)return FALSE;
 SendMessage(replacement,LB_SETITEMHEIGHT,0,(LPARAM)16);
 SendMessage(replacement,LB_SETHORIZONTALEXTENT,Z7_COL_END+12,0);
 fileList=replacement;fileListMultiple=multiple;
 if(old){SetWindowPos(replacement,old,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);DestroyWindow(old);}
 if(focused)SetFocus(replacement);return TRUE;
}
static long singleListSelection(void){
 int selected;
 if(!fileListMultiple)return SendMessage(fileList,LB_GETCURSEL,0,0);
 if(SendMessage(fileList,LB_GETSELCOUNT,0,0)!=1||SendMessage(fileList,LB_GETSELITEMS,1,(LPARAM)(LPINT)&selected)!=1)return -1;
 return selected;
}
static void selectListItem(int index){
 selectionAnchor=-1;
 if(fileListMultiple){SendMessage(fileList,LB_SETSEL,FALSE,-1);if(index>=0){SendMessage(fileList,LB_SETSEL,TRUE,index);SendMessage(fileList,LB_SETCARETINDEX,index,FALSE);}}
 else SendMessage(fileList,LB_SETCURSEL,index,0);
}
static const char *operationText="Testing";
static void updateToolbarState(void){
 BOOL available=(opened||folderView)&&!busy;long selected;BOOL alter;
 if(!IsWindow(mainWindow))return;
 selected=singleListSelection();alter=available&&((fileListMultiple&&SendMessage(fileList,LB_GETSELCOUNT,0,0)>1)||(selected>=0&&(!folderView||selected!=0||GetWindowTextLength(pathEdit)==3)));
 EnableWindow(copyButton,folderView&&alter);EnableWindow(moveButton,folderView&&alter);EnableWindow(deleteButton,alter);EnableWindow(infoButton,available);
}

static void closeArchive(void){
 folderView=FALSE;
 if(opened){SzArEx_Free(&archive,&allocator);fclose(input.file);opened=0;}
 SendMessage(fileList,LB_RESETCONTENT,0,0);EnableWindow(testButton,FALSE);EnableWindow(extractButton,FALSE);updateToolbarState();
}
static void nameText(UInt32 index,char *text,unsigned capacity){
 size_t p=archive.FileNameOffsets[index],end=archive.FileNameOffsets[index+1];unsigned used=0;
 for(;p+1<end;p++){
  unsigned ch=archive.FileNames[p*2]|((unsigned)archive.FileNames[p*2+1]<<8);
  if(used+7>=capacity)break;
  if(ch>=32 && ch<127 && ch!='\\')text[used++]=(char)ch;
  else {sprintf(text+used,"\\u%04X",ch);used+=6;}
 }
 if(p+1<end && used+4<capacity){strcpy(text+used,"...");used+=3;}
 text[used]=0;
}
static void sizeText(UInt64 value,char *out){char reverse[24];unsigned n=0,i;do{reverse[n++]=(char)('0'+value%10);value/=10;}while(value);for(i=0;i<n;i++)out[i]=reverse[n-1-i];out[n]=0;}
/* Compact modified-time string "YYYY-MM-DD HH:MM" (shared with properties). */
static void ntfsTimeText(const CNtfsFileTime *time,char *out){
 static const unsigned monthDays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
 UInt64 seconds=(((UInt64)time->High<<32)|time->Low)/10000000UL;UInt32 days=(UInt32)(seconds/86400UL),within=(UInt32)(seconds%86400UL);unsigned year,month,span,leap;
 year=(unsigned)(1601UL+400UL*(days/146097UL));days%=146097UL;
 for(;;){leap=!(year%4)&&((year%100)||!(year%400));span=365+leap;if(days<span)break;days-=span;year++;}
 for(month=0;month<11;month++){span=monthDays[month]+(month==1?leap:0);if(days<span)break;days-=span;}
 sprintf(out,"%04u-%02u-%02u %02u:%02u",year,month+1,(unsigned)days+1,(unsigned)(within/3600),(unsigned)(within/60%60));
}
/* DOS packed date/time -> "YYYY-MM-DD HH:MM". */
static void dosDateText(unsigned wrdate,unsigned wrtime,char *out){
 if(!wrdate){out[0]=0;return;}
 sprintf(out,"%04u-%02u-%02u %02u:%02u",1980+(wrdate>>9),(wrdate>>5)&15,wrdate&31,wrtime>>11,(wrtime>>5)&63);
}
/* FAT attribute bits -> compact letters (same bit layout for 7z and _dos). */
static void attrText(unsigned a,char *out){
 unsigned n=0;
 if(a&0x10)out[n++]='D';if(a&0x01)out[n++]='R';if(a&0x02)out[n++]='H';if(a&0x04)out[n++]='S';if(a&0x20)out[n++]='A';
 out[n]=0;
}
static void loadArchive(void){
 static char path[260],line[360],name[260],size[24],date[24],attr[8];UInt32 i;SRes result;long row;int isdir;
 if(busy)return;closeArchive();selectionAnchor=-1;if(!configureFileList(TRUE)){lastResult=SZ_ERROR_MEM;SetWindowText(statusText,errorText(lastResult));return;}GetWindowText(pathEdit,path,sizeof(path));
 input.iface.Read=readInput;input.iface.Seek=seekInput;input.file=fopen(path,"rb");
 if(!input.file){lastResult=SZ_ERROR_READ;SetWindowText(statusText,"Cannot open the archive.");return;}
 LookToRead2_CreateVTable(&look,False);look.realStream=&input.iface;look.buf=lookBuffer;look.bufSize=lookCapacity;LookToRead2_INIT(&look);SzArEx_Init(&archive);
 traceRegisters();trace("before open");result=openArchive(&archive,&input,&look,&allocator);trace("after open");traceRegisters();lastResult=result;
 if(result!=SZ_OK){SzArEx_Free(&archive,&allocator);fclose(input.file);SetWindowText(statusText,errorText(result));return;}
 opened=1;
 
 for(i=0;i<archive.NumFiles;i++){
  trace("before name");nameText(i,name,sizeof(name));isdir=SzArEx_IsDir(&archive,i);
  trace("before size");if(isdir)size[0]=0;else sizeText(SzArEx_GetFileSize(&archive,i),size);
  date[0]=0;if(SzBitWithVals_Check(&archive.MTime,i))ntfsTimeText(archive.MTime.Vals+i,date);
  attr[0]=0;if(SzBitWithVals_Check(&archive.Attribs,i))attrText((unsigned)archive.Attribs.Vals[i],attr);else if(isdir)strcpy(attr,"D");
  trace("before format");sprintf(line,"%s\t%s\t%s\t%s",name,size,date,attr);
  trace("before list add");trace(line);row=SendMessage(fileList,LB_ADDSTRING,0,(LPARAM)(LPSTR)line);if(row<0){lastResult=SZ_ERROR_MEM;break;}
  SendMessage(fileList,LB_SETITEMDATA,(WPARAM)row,(LPARAM)(isdir?Z7_ITEM_DIR:Z7_ITEM_FILE));
 }
 InvalidateRect(fileList,NULL,TRUE);
 if(lastResult!=SZ_OK){closeArchive();SetWindowText(statusText,errorText(lastResult));return;}
 trace("before status");sprintf(line,"%lu items. Ctrl/Shift select items; Ctrl+A selects all. Non-ASCII names use Unicode escapes.",archive.NumFiles);SetWindowText(statusText,line);EnableWindow(testButton,TRUE);EnableWindow(extractButton,TRUE);updateToolbarState();trace("load complete");
}
#include "filesystem_view.h"
#ifdef __WATCOMC__
#pragma code_seg("CREATE_TEXT")
#endif
static int progress(UInt64 position){
 MSG message;char size[24],text[80];
 updateToolbarState();
 trace("progress entered");
 if(dictionaryCancelAfter&&position>=dictionaryCancelAfter){PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);dictionaryCancelAfter=0;}
 if(renameTesting&&renameCancelAfterWrite&&position){PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);renameCancelAfterWrite=FALSE;}
 if(hashCancelAfterRead&&position){PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);hashCancelAfterRead=FALSE;}
 if(createTesting&&createCancelAfterRead&&position){PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);createCancelAfterRead=FALSE;}
 trace("progress before size");sizeText(position,size);trace("progress after size");sprintf(text,"%s... %s bytes",operationText,size);trace("progress before status");SetWindowText(statusText,text);trace("progress after status");
 while(PeekMessage(&message,0,0,0,PM_REMOVE)){
  if(message.message==WM_QUIT){cancelled=1;closePending=1;break;}
  trace("progress message");TranslateMessage(&message);DispatchMessage(&message);trace("progress dispatched");
 }
 trace("progress returning");return !cancelled;
}
static void performTest(void){
 if(!opened||busy)return;operationText="Testing";busy=1;cancelled=0;closePending=0;
 EnableWindow(openButton,FALSE);EnableWindow(createButton,FALSE);EnableWindow(pathEdit,FALSE);EnableWindow(testButton,FALSE);EnableWindow(extractButton,FALSE);EnableWindow(cancelButton,TRUE);
 testProgress=progress;lastResult=testArchive(&archive,&input,&look,&allocator);testProgress=NULL;
 SetWindowText(statusText,lastResult==SZ_OK?"Everything is Ok":errorText(lastResult));
 busy=0;EnableWindow(openButton,TRUE);EnableWindow(createButton,TRUE);EnableWindow(pathEdit,TRUE);EnableWindow(testButton,TRUE);EnableWindow(extractButton,TRUE);EnableWindow(cancelButton,FALSE);
 if(closePending)PostMessage(mainWindow,WM_CLOSE,0,0);
}
static SRes archiveGuiSelection(Byte **mask,size_t *bytes){
 long count,chosen;UInt32 i;SRes result;
 *mask=NULL;*bytes=0;count=SendMessage(fileList,LB_GETSELCOUNT,0,0);
 if(count<0||SendMessage(fileList,LB_GETCOUNT,0,0)!=(long)archive.NumFiles)return SZ_ERROR_PARAM;
 if(!count)return SZ_OK; /* With no selection, extract the entire archive. */
 result=archiveSelectionAllocate(&archive,&allocator,mask,bytes);if(result!=SZ_OK)return result;
 for(i=0;i<archive.NumFiles;i++){
  chosen=SendMessage(fileList,LB_GETSEL,(WPARAM)i,0);
  if(chosen<0){ISzAlloc_Free(&allocator,*mask);*mask=NULL;*bytes=0;return SZ_ERROR_PARAM;}
  if(chosen)archiveSelectionAdd(&archive,*mask,i);
 }
 return SZ_OK;
}
static void performExtract(void){
 FARPROC proc;int choice;Byte *selection=NULL;size_t selectionBytes=0;
 if(!opened||busy)return;
 proc=MakeProcInstance((FARPROC)ExtractProc,instance);if(!proc)return;
 choice=DialogBox(instance,MAKEINTRESOURCE(300),mainWindow,(DLGPROC)proc);FreeProcInstance(proc);
 if(choice!=1)return;
 extractMessage[0]=0;lastResult=archiveGuiSelection(&selection,&selectionBytes);
 if(lastResult!=SZ_OK){SetWindowText(statusText,errorText(lastResult));return;}
 busy=1;cancelled=0;closePending=0;operationText="Extracting";
 EnableWindow(openButton,FALSE);EnableWindow(createButton,FALSE);EnableWindow(pathEdit,FALSE);EnableWindow(testButton,FALSE);EnableWindow(extractButton,FALSE);EnableWindow(cancelButton,TRUE);
 testProgress=progress;lastResult=extractArchiveSelected(&archive,&input,&look,&allocator,extractDestination,extractFlat,selection,selectionBytes);testProgress=NULL;
 ISzAlloc_Free(&allocator,selection);
 SetWindowText(statusText,lastResult==SZ_OK?"Extraction complete. Original metadata remains in the source archive.":errorText(lastResult));
 busy=0;EnableWindow(openButton,TRUE);EnableWindow(createButton,TRUE);EnableWindow(pathEdit,TRUE);EnableWindow(testButton,TRUE);EnableWindow(extractButton,TRUE);EnableWindow(cancelButton,FALSE);
 if(extractMessage[0]&&!extractTesting)MessageBox(mainWindow,extractMessage,"Extract",MB_OK|MB_ICONEXCLAMATION);
 if(closePending)PostMessage(mainWindow,WM_CLOSE,0,0);
}
#include "hash_gui.h"
static void performCreate(int updating){
 FARPROC proc;int choice,count;const char *sources[256];SRes operation;const char *message;char *createSourceBuffer;
 if(busy||(updating&&!opened))return;createUpdating=updating;
 if(updating)GetWindowText(pathEdit,createDestination,sizeof(createDestination));
 proc=MakeProcInstance((FARPROC)CreateProc,instance);if(!proc)return;
 choice=DialogBox(instance,MAKEINTRESOURCE(400),mainWindow,(DLGPROC)proc);FreeProcInstance(proc);
 if(choice!=1)return;
 createSourceBuffer=(char *)ISzAlloc_Alloc(&allocator,strlen(createSource)+1);
 if(!createSourceBuffer){lastResult=SZ_ERROR_MEM;SetWindowText(statusText,errorText(lastResult));return;}
 strcpy(createSourceBuffer,createSource);count=splitSourceList(createSourceBuffer,sources,256);
 if(count<=0){ISzAlloc_Free(&allocator,createSourceBuffer);lastResult=SZ_ERROR_UNSUPPORTED;MessageBox(mainWindow,"Enter file or folder paths, one per line. Check any quotation marks.","Create archive",MB_OK|MB_ICONEXCLAMATION);return;}
 closeArchive(); /* Release the old archive's buffers before encoder allocation. */
 busy=1;cancelled=0;closePending=0;operationText=updating?"Updating":"Creating";
 EnableWindow(openButton,FALSE);EnableWindow(createButton,FALSE);EnableWindow(pathEdit,FALSE);EnableWindow(testButton,FALSE);EnableWindow(extractButton,FALSE);EnableWindow(cancelButton,TRUE);
 if(createTesting&&createCancelTesting)PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);
 testProgress=progress;
 if(createFormat>=2&&!updating){
  /* Single-stream formats: gzip/xz take exactly one file; tar stores many. */
  if(createFormat==3)operation=tarCreate(createDestination,(unsigned)count,sources,&allocator);
  else if(count!=1){operation=SZ_ERROR_PARAM;strcpy(createFormat==2?gzipMessage:xzMessage,createFormat==2?"ERROR: gzip stores exactly one file.":"ERROR: xz stores exactly one file.");}
  else if(createFormat==2)operation=gzipCreate(createDestination,sources[0],&allocator);
  else{
#ifdef Z7_WIN16_WRITE_XZ_AES
   operation=xzCreate(createDestination,sources[0],&allocator);
#else
   operation=SZ_ERROR_UNSUPPORTED;
#endif
  }
 }else operation=updating?(createZipMode?updateZip(createDestination,(unsigned)count,sources,&allocator,createMethod,0):updateAnyArchive(createDestination,(unsigned)count,sources,&allocator,createMethod==1,0)):createZipMode?createZip(createDestination,(unsigned)count,sources,&allocator,createMethod):createArchiveMethod(createDestination,(unsigned)count,sources,&allocator,createMethod==1);
 testProgress=NULL;ISzAlloc_Free(&allocator,createSourceBuffer);lastResult=operation;
#ifdef Z7_WIN16_WRITE_XZ_AES
 aes7zEncSetPassword((const char *)0);   /* do not carry the password to the next create */
#endif
 busy=0;EnableWindow(openButton,TRUE);EnableWindow(createButton,TRUE);EnableWindow(pathEdit,TRUE);EnableWindow(testButton,opened);EnableWindow(extractButton,opened);EnableWindow(cancelButton,FALSE);
 if((lastResult==SZ_OK||updating)&&createFormat<2){SetWindowText(pathEdit,createDestination);loadArchive();if(operation!=SZ_OK)lastResult=operation;}
 else if(lastResult==SZ_OK&&createFormat>=2){
  /* Stream formats are not browsed in the 7z/zip list; show the directory. */
  char dir[260],full[260],*slash;strcpy(dir,createDestination);
  slash=strrchr(dir,'\\');if(!slash)slash=strrchr(dir,'/');
  if(slash){if(slash==dir+2)slash[1]=0;else *slash=0;}
  if(directoryPath(dir,full))loadDirectory(full);
 }
 if(createFormat>=2&&!updating){
  if(lastResult==SZ_OK)strcpy(createFormat==2?gzipMessage:createFormat==3?tarMessage:xzMessage,
   createFormat==2?"gzip archive created (Deflate).":createFormat==3?"tar archive created (store).":"xz archive created (LZMA2, 8 KiB dictionary).");
  message=createFormat==2?gzipMessage:createFormat==3?tarMessage:xzMessage;
 }else message=updating?updateMessage:createZipMode?zipCreateMessage:createMessage;
 SetWindowText(statusText,lastResult==SZ_OK?message:errorText(lastResult));
 if(message[0]&&!createTesting&&(lastResult!=SZ_OK||strstr(message,"WARNING:")))MessageBox(mainWindow,message,updating?"Update archive":"Create archive",MB_OK|MB_ICONEXCLAMATION);
 if(closePending)PostMessage(mainWindow,WM_CLOSE,0,0);
}
static void performRename(void){
 FARPROC proc;int choice;long selected;size_t i,start,end;unsigned used=0;char path[260];SRes operation;
 if(!opened||busy)return;
 selected=singleListSelection();
 if(selected<0||(UInt32)selected>=archive.NumFiles){if(!renameTesting)MessageBox(mainWindow,"Select one archive entry first.","Rename",MB_OK);return;}
 start=archive.FileNameOffsets[selected];end=archive.FileNameOffsets[selected+1];
 for(i=start;i+1<end;i++){
  unsigned ch=renameChar(archive.FileNames+2*i);
  if(ch>=127||used>=sizeof(renameOld)-1){lastResult=SZ_ERROR_UNSUPPORTED;if(!renameTesting)MessageBox(mainWindow,"This rename command currently requires an ASCII source path.","Rename",MB_OK);return;}
  renameOld[used++]=(char)ch;
 }
 renameOld[used]=0;if(!renameTesting)strcpy(renameNew,renameOld);
 proc=MakeProcInstance((FARPROC)RenameProc,instance);if(!proc)return;
 choice=DialogBox(instance,MAKEINTRESOURCE(500),mainWindow,(DLGPROC)proc);FreeProcInstance(proc);if(choice!=1)return;
 GetWindowText(pathEdit,path,sizeof(path));closeArchive();
 busy=1;cancelled=0;closePending=0;operationText="Renaming";
 EnableWindow(openButton,FALSE);EnableWindow(createButton,FALSE);EnableWindow(pathEdit,FALSE);EnableWindow(cancelButton,TRUE);
 testProgress=progress;operation=renameAnyArchive(path,renameOld,renameNew,&allocator);testProgress=NULL;
 busy=0;EnableWindow(openButton,TRUE);EnableWindow(createButton,TRUE);EnableWindow(pathEdit,TRUE);EnableWindow(cancelButton,FALSE);
 loadArchive();if(opened&&selected<(long)archive.NumFiles)selectListItem((int)((WPARAM)selected));
 if(operation!=SZ_OK)lastResult=operation;
 SetWindowText(statusText,lastResult==SZ_OK?renameMessage:errorText(lastResult));
 if(lastResult!=SZ_OK&&renameMessage[0]&&!renameTesting)MessageBox(mainWindow,renameMessage,"Rename archive entry",MB_OK|MB_ICONEXCLAMATION);
 if(closePending)PostMessage(mainWindow,WM_CLOSE,0,0);
}
static int deleteTestChoice=IDYES;
static void performDelete(void){
 long count,selected;UInt32 row;size_t i,end;unsigned used,n=0;char name[260],path[260],question[360];char **names=NULL;SRes operation=SZ_ERROR_PARAM;
 if(!opened||busy)return;
 count=SendMessage(fileList,LB_GETSELCOUNT,0,0);
 if(count<=0||count>256){lastResult=SZ_ERROR_PARAM;SetWindowText(statusText,"Select between 1 and 256 archive entries to delete.");return;}
 names=(char **)ISzAlloc_Alloc(&allocator,(unsigned)count*sizeof(char *));
 if(!names){operation=SZ_ERROR_MEM;goto rejected;}
 memset(names,0,(unsigned)count*sizeof(char *));
 /* Retain complete names before closing the archive. List rows are only a
    display: escaped/truncated labels must never be used as update paths. */
 for(row=0;row<archive.NumFiles;row++){
  selected=SendMessage(fileList,LB_GETSEL,(WPARAM)row,0);
  if(selected<0)goto rejected;if(!selected)continue;if(n>=(unsigned)count)goto rejected;
  used=0;end=archive.FileNameOffsets[row+1];
  for(i=archive.FileNameOffsets[row];i+1<end;i++){
   unsigned ch=renameChar(archive.FileNames+2*i);
   if(ch>=127||used>=sizeof(name)-1){operation=SZ_ERROR_UNSUPPORTED;goto rejected;}
   name[used++]=(char)ch;
  }
  name[used]=0;names[n]=(char *)ISzAlloc_Alloc(&allocator,used+1);
  if(!names[n]){operation=SZ_ERROR_MEM;goto rejected;}strcpy(names[n++],name);
 }
 if(n!=(unsigned)count)goto rejected;
 sprintf(question,"Delete %u selected archive entries? All contents of selected folders will also be deleted.",n);
 if((deleteTesting?deleteTestChoice:MessageBox(mainWindow,question,"Delete archive entries",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2))!=IDYES){operation=SZ_ERROR_PROGRESS;goto rejected;}
 GetWindowText(pathEdit,path,sizeof(path));closeArchive();
 busy=1;cancelled=0;closePending=0;operationText="Deleting";
 EnableWindow(openButton,FALSE);EnableWindow(createButton,FALSE);EnableWindow(pathEdit,FALSE);EnableWindow(cancelButton,TRUE);
 testProgress=progress;operation=updateAnyArchive(path,n,(const char *const *)names,&allocator,1,1);testProgress=NULL;
 for(i=0;i<n;i++)ISzAlloc_Free(&allocator,names[i]);ISzAlloc_Free(&allocator,names);names=NULL;
 busy=0;EnableWindow(openButton,TRUE);EnableWindow(createButton,TRUE);EnableWindow(pathEdit,TRUE);EnableWindow(cancelButton,FALSE);
 loadArchive();if(operation!=SZ_OK)lastResult=operation;
 SetWindowText(statusText,lastResult==SZ_OK?updateMessage:errorText(lastResult));
 if(updateMessage[0]&&!deleteTesting&&(lastResult!=SZ_OK||strstr(updateMessage,"WARNING:")))MessageBox(mainWindow,updateMessage,"Delete archive entries",MB_OK|MB_ICONEXCLAMATION);
 if(closePending)PostMessage(mainWindow,WM_CLOSE,0,0);return;
rejected:
 if(names){for(i=0;i<n;i++)ISzAlloc_Free(&allocator,names[i]);ISzAlloc_Free(&allocator,names);}
 lastResult=operation;SetWindowText(statusText,operation==SZ_ERROR_UNSUPPORTED?"Selected names must fit ASCII archive paths. The archive is unchanged.":errorText(operation));
}

#ifdef __WATCOMC__
#pragma code_seg()
#endif
#include "filesystem_gui.h"
#include "properties_gui.h"
#include "list_render.h"
/* Keep the growing integration fixtures outside the main 64 KiB code
   segment. Large-model calls remain valid in real and protected modes. */
#ifdef __WATCOMC__
#pragma code_seg("GUI_TEST_TEXT")
#endif
static int creationSelfTest(FILE *log){
 int valid=1,created;struct stat info;FILE *empty;
 empty=fopen("C:\\C32ZERO.TXT","wb");if(!empty)return 0;if(fclose(empty))return 0;
 createTesting=TRUE;createMethod=1;if(!setCreateSource("C:\\C31IN.TXT\r\n\"C:\\C32ZERO.TXT\"\r\n"))return 0;strcpy(createDestination,"C:\\G31NEW.7Z");
 SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);created=lastResult==SZ_OK;
 valid&=created&&opened&&archive.NumFiles==2&&SzArEx_GetFileSize(&archive,0)==102400UL&&SzArEx_GetFileSize(&archive,1)==0;
 if(created){valid&=SzBitWithVals_Check(&archive.CRCs,0)&&archive.CRCs.Vals[0]==0x9a0e0c8cUL;performTest();valid&=lastResult==SZ_OK;}
 valid&=!stat(createDestination,&info)&&info.st_size<102400L;
 fprintf(log,"%s LZMA Create dialog with multiple sources; compressed 102400-byte file and empty file reopen and test\n",valid?"PASS":"FAIL");
 SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);valid&=lastResult==SZ_ERROR_WRITE;
 fprintf(log,"%s existing archive is preserved during creation\n",lastResult==SZ_ERROR_WRITE?"PASS":"FAIL");
 closeArchive();if(created&&remove(createDestination))valid=0;
 strcpy(createDestination,"C:\\G31CANC.7Z");createCancelTesting=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);
 valid&=lastResult==SZ_ERROR_PROGRESS&&stat(createDestination,&info)!=0;
 fprintf(log,"%s creation cancellation leaves no archive\n",lastResult==SZ_ERROR_PROGRESS?"PASS":"FAIL");
 createCancelTesting=FALSE;createCancelAfterRead=TRUE;strcpy(createDestination,"C:\\G34CANC.7Z");
 SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);
 valid&=lastResult==SZ_ERROR_PROGRESS&&stat(createDestination,&info)!=0&&!windowAllocationCount;
 fprintf(log,"%s cancellation during LZMA input; no partial archive or outstanding encoder blocks\n",lastResult==SZ_ERROR_PROGRESS&&!windowAllocationCount?"PASS":"FAIL");
 createCancelAfterRead=FALSE;createTesting=FALSE;if(remove("C:\\C32ZERO.TXT"))valid=0;return valid;
}
static int verifyExtractedFile(const char *path,UInt64 expected,UInt32 expectedCrc){
 FILE *file=fopen(path,"rb");Byte buffer[256];size_t n;UInt64 size=0;UInt32 crc=CRC_INIT_VAL;int valid;
 if(!file)return 0;
 while((n=fread(buffer,1,sizeof(buffer),file))!=0){size+=n;crc=CrcUpdate(crc,buffer,n);}
 valid=!ferror(file)&&size==expected&&CRC_GET_DIGEST(crc)==expectedCrc;
 if(fclose(file))valid=0;return valid;
}
static int extractionSelfTest(FILE *log){
 int valid=1,created;struct stat info;
 extractTesting=TRUE;strcpy(extractDestination,"C:\\G29OUT");SetWindowText(pathEdit,"C:\\EXTRACT.7Z");loadArchive();
 if(!opened||lastResult!=SZ_OK){extractTesting=FALSE;fputs("FAIL extraction fixture opens\n",log);return 0;}
 SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);created=lastResult==SZ_OK;valid&=created;
 valid&=verifyExtractedFile("C:\\G29OUT\\FIRST.TXT",240000UL,0x4bd252cbUL);
 valid&=verifyExtractedFile("C:\\G29OUT\\SUB\\DEEP\\SECOND.BIN",102400UL,0x9a0e0c8cUL);
 valid&=verifyExtractedFile("C:\\G29OUT\\ZERO.TXT",0,0);
 valid&=!stat("C:\\G29OUT\\EMPTY",&info)&&(info.st_mode&S_IFDIR);
 fprintf(log,"%s Extract dialog and command; solid files, nested and empty directories, exact file CRCs\n",valid?"PASS":"FAIL");
 SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);
 if(lastResult!=SZ_ERROR_WRITE||!verifyExtractedFile("C:\\G29OUT\\FIRST.TXT",240000UL,0x4bd252cbUL))valid=0;
 fprintf(log,"%s existing extraction destination is preserved; result=%d\n",lastResult==SZ_ERROR_WRITE?"PASS":"FAIL",lastResult);
 if(created){
  if(remove("C:\\G29OUT\\FIRST.TXT")||remove("C:\\G29OUT\\SUB\\DEEP\\SECOND.BIN")||remove("C:\\G29OUT\\ZERO.TXT"))valid=0;
  if(rmdir("C:\\G29OUT\\SUB\\DEEP")||rmdir("C:\\G29OUT\\SUB")||rmdir("C:\\G29OUT\\EMPTY")||rmdir("C:\\G29OUT"))valid=0;
 }
 strcpy(extractDestination,"C:\\G29BAD");SetWindowText(pathEdit,"C:\\MULTI.7Z");loadArchive();SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);
 if(lastResult!=SZ_ERROR_UNSUPPORTED||!stat(extractDestination,&info))valid=0;
 fprintf(log,"%s unrepresentable DOS filename rejected before output; result=%d\n",lastResult==SZ_ERROR_UNSUPPORTED?"PASS":"FAIL",lastResult);
 strcpy(extractDestination,"C:\\G29CRC");SetWindowText(pathEdit,"C:\\BADCRC.7Z");loadArchive();SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);
 if(lastResult!=SZ_ERROR_CRC||!stat(extractDestination,&info))valid=0;
 fprintf(log,"%s corrupted archive rejected before output; result=%d\n",lastResult==SZ_ERROR_CRC?"PASS":"FAIL",lastResult);
 extractTesting=FALSE;return valid;
}
static int metadataSelfTest(FILE *log){
 int valid=1;struct stat info;unsigned attributes=0;
 extractTesting=TRUE;strcpy(extractDestination,"C:\\G32META");SetWindowText(pathEdit,"C:\\META32.7Z");loadArchive();
 if(!opened||lastResult!=SZ_OK){extractTesting=FALSE;fputs("FAIL metadata fixture opens\n",log);return 0;}
 SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);
 valid=lastResult==SZ_OK&&!extractMessage[0];
 valid&=!stat("C:\\G32META\\RONLY.TXT",&info)&&info.st_mtime==946684800UL;
 valid&=!_dos_getfileattr("C:\\G32META\\RONLY.TXT",&attributes)&&(attributes&1)!=0;
 valid&=!stat("C:\\G32META\\MFILE.TXT",&info)&&info.st_mtime==946684800UL;
 fprintf(log,"%s extracted modification timestamps and DOS read-only attribute\n",valid?"PASS":"FAIL");
 if(extractMessage[0])fprintf(log,"Metadata message: %s\n",extractMessage);
 if(lastResult==SZ_OK){
  if(_dos_setfileattr("C:\\G32META\\RONLY.TXT",0)||remove("C:\\G32META\\RONLY.TXT")||remove("C:\\G32META\\MFILE.TXT")||rmdir("C:\\G32META"))valid=0;
 }
 extractTesting=FALSE;return valid;
}
static int deflateSelfTest(FILE *log){
 int valid;struct stat info;
 SetWindowText(pathEdit,"C:\\DEFLATE.7Z");loadArchive();
 valid=opened&&lastResult==SZ_OK;
 if(!valid){fputs("FAIL Deflate fixture opens\n",log);return 0;}
 SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);valid&=lastResult==SZ_OK;
 PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);
 valid&=lastResult==SZ_ERROR_PROGRESS;
 SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);valid&=lastResult==SZ_OK;
 extractTesting=TRUE;strcpy(extractDestination,"C:\\G35DEF");
 SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);extractTesting=FALSE;
 valid&=lastResult==SZ_OK&&verifyExtractedFile("C:\\G35DEF\\DEFLATE.BIN",306000UL,0x2783538aUL);
 if(lastResult==SZ_OK){if(remove("C:\\G35DEF\\DEFLATE.BIN")||rmdir("C:\\G35DEF"))valid=0;}
 fprintf(log,"%s streaming Deflate, 32 KiB history, cancellation, repeat and exact extraction\n",valid?"PASS":"FAIL");
 SetWindowText(pathEdit,"C:\\DEFBAD.7Z");loadArchive();
 if(!opened)return 0;
 extractTesting=TRUE;strcpy(extractDestination,"C:\\G35BAD");
 SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);extractTesting=FALSE;
 if(lastResult==SZ_OK||!stat("C:\\G35BAD",&info))valid=0;
 fprintf(log,"%s damaged Deflate rejected before output\n",valid?"PASS":"FAIL");
 closeArchive();if(windowAllocationCount)valid=0;
 return valid;
}
static int zipSelfTest(FILE *log){
 int valid;struct stat info;FILE *dated;unsigned date=0,time=0;
 SetWindowText(pathEdit,"C:\\WIN16.ZIP");loadArchive();
 valid=opened&&lastResult==SZ_OK&&archive.NumFiles==3;
 if(!valid){fputs("FAIL ZIP fixture opens\n",log);return 0;}
 SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);valid&=lastResult==SZ_OK;
 PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);valid&=lastResult==SZ_ERROR_PROGRESS;
 SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);valid&=lastResult==SZ_OK;
 extractTesting=TRUE;strcpy(extractDestination,"C:\\G36ZIP");SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);extractTesting=FALSE;
 valid&=lastResult==SZ_OK&&strstr(extractMessage,"directory modification times")!=NULL;
 dated=fopen("C:\\G36ZIP\\DEFLATE.BIN","rb");
 if(!dated)valid=0;else{if(_dos_getftime(fileno(dated),&date,&time)||date!=10273||time!=0)valid=0;fclose(dated);}
 fprintf(log,"ZIP metadata: file DOS date=%u time=%u; %s\n",date,time,extractMessage);
 valid&=verifyExtractedFile("C:\\G36ZIP\\DEFLATE.BIN",306000UL,0x2783538aUL);
 valid&=verifyExtractedFile("C:\\G36ZIP\\SUB\\ZERO.TXT",0,0);
 valid&=!stat("C:\\G36ZIP\\EMPTY",&info)&&(info.st_mode&S_IFDIR);
 if(lastResult==SZ_OK){
  if(remove("C:\\G36ZIP\\DEFLATE.BIN")||remove("C:\\G36ZIP\\SUB\\ZERO.TXT")||rmdir("C:\\G36ZIP\\SUB")||rmdir("C:\\G36ZIP\\EMPTY")||rmdir("C:\\G36ZIP"))valid=0;
 }
 fprintf(log,"%s ZIP list/test/cancel/repeat, exact extraction, directories and metadata\n",valid?"PASS":"FAIL");
 SetWindowText(pathEdit,"C:\\ZIPBAD.ZIP");loadArchive();
 if(!opened)return 0;
 extractTesting=TRUE;strcpy(extractDestination,"C:\\G36BAD");SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);extractTesting=FALSE;
 if(lastResult==SZ_OK||!stat("C:\\G36BAD",&info))valid=0;
 fprintf(log,"%s damaged ZIP rejected before output\n",valid?"PASS":"FAIL");
 closeArchive();if(windowAllocationCount)valid=0;return valid;
}
static int renameIdentity(const char *path,UInt32 *crc,UInt32 *size){
 FILE *f=fopen(path,"rb");Byte buffer[512];size_t n;int good;
 if(!f)return 0;*crc=CRC_INIT_VAL;*size=0;
 while((n=fread(buffer,1,sizeof(buffer),f))!=0){*crc=CrcUpdate(*crc,buffer,n);*size+=(UInt32)n;}
 good=!ferror(f);fclose(f);*crc=CRC_GET_DIGEST(*crc);return good;
}
static int renameSelfTest(FILE *log){
 FILE *source,*destination;Byte buffer[512];size_t n;unsigned i;int valid=1;UInt32 crc,size;
 source=fopen("C:\\RN37.7Z","rb");if(!source)return 0;destination=fopen("C:\\G37RN.7Z","wb");if(!destination){fclose(source);return 0;}
 while((n=fread(buffer,1,sizeof(buffer),source))!=0)if(fwrite(buffer,1,n,destination)!=n){valid=0;break;}
 if(ferror(source)||fclose(destination))valid=0;fclose(source);if(!valid)return 0;
 SetWindowText(pathEdit,"C:\\G37RN.7Z");loadArchive();if(!opened)return 0;
 for(i=0;i<archive.NumFiles;i++)if(renameMatch(archive.FileNames+2*archive.FileNameOffsets[i],"DIR",0))break;
 if(i==archive.NumFiles){closeArchive();return 0;}
 selectListItem((int)(i));renameTesting=TRUE;strcpy(renameNew,"MOVED");SendMessage(mainWindow,WM_COMMAND,ID_RENAME,0);
 valid=lastResult==SZ_OK&&opened;
 if(valid){
  valid&=renameMatch(archive.FileNames+2*archive.FileNameOffsets[i],"MOVED",0);
  SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);valid&=lastResult==SZ_OK;
 }
 fprintf(log,"%s Rename dialog and folder subtree update; encoded header and data CRCs\n",valid?"PASS":"FAIL");
 if(!renameIdentity("C:\\G37RN.7Z",&crc,&size))valid=0;
 renameCancelAfterWrite=TRUE;strcpy(renameNew,"CANCEL");SendMessage(mainWindow,WM_COMMAND,ID_RENAME,0);
 valid&=lastResult==SZ_ERROR_PROGRESS&&verifyExtractedFile("C:\\G37RN.7Z",size,crc);
 fprintf(log,"%s cancelled update preserves exact original archive bytes\n",valid?"PASS":"FAIL");
 strcpy(renameNew,"KEEP.TXT");SendMessage(mainWindow,WM_COMMAND,ID_RENAME,0);
 valid&=lastResult==SZ_ERROR_UNSUPPORTED&&verifyExtractedFile("C:\\G37RN.7Z",size,crc);
 fprintf(log,"%s rename collision rejected without changing the archive\n",valid?"PASS":"FAIL");
 renameTesting=FALSE;renameCancelAfterWrite=FALSE;closeArchive();if(windowAllocationCount)valid=0;if(remove("C:\\G37RN.7Z"))valid=0;
 return valid;
}
static int updateSelfTest(FILE *log){
 FILE *src,*dst;Byte buffer[512];size_t n;unsigned i;int valid=1;UInt32 crc,size;
 src=fopen("C:\\RN37.7Z","rb");if(!src)return 0;dst=fopen("C:\\G38UP.7Z","wb");if(!dst){fclose(src);return 0;}
 while((n=fread(buffer,1,sizeof(buffer),src))!=0)if(fwrite(buffer,1,n,dst)!=n){valid=0;break;}
 if(ferror(src)||fclose(dst))valid=0;fclose(src);if(!valid)return 0;
 src=fopen("C:\\C31IN.TXT","rb");if(!src)return 0;dst=fopen("C:\\KEEP.TXT","wb");if(!dst){fclose(src);return 0;}
 while((n=fread(buffer,1,sizeof(buffer),src))!=0)if(fwrite(buffer,1,n,dst)!=n){valid=0;break;}
 if(ferror(src)||fclose(dst))valid=0;fclose(src);if(!valid)return 0;
 SetWindowText(pathEdit,"C:\\G38UP.7Z");loadArchive();if(!opened)return 0;
 createTesting=TRUE;createMethod=1;if(!setCreateSource("C:\\KEEP.TXT\r\nC:\\C31IN.TXT\r\n"))return 0;
 SendMessage(mainWindow,WM_COMMAND,ID_UPDATE,0);valid=lastResult==SZ_OK&&opened&&archive.NumFiles==4;
 if(valid){performTest();valid&=lastResult==SZ_OK;}
 fprintf(log,"%s Add/replace dialog; LZMA update of a solid archive and data verification (result %d)\n",valid?"PASS":"FAIL",lastResult);
 if(!renameIdentity("C:\\G38UP.7Z",&crc,&size))valid=0;
 createCancelAfterRead=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_UPDATE,0);
 valid&=lastResult==SZ_ERROR_PROGRESS&&verifyExtractedFile("C:\\G38UP.7Z",size,crc);
 fprintf(log,"%s cancelled content update preserves exact original bytes\n",valid?"PASS":"FAIL");
 createCancelAfterRead=FALSE;createTesting=FALSE;
 for(i=0;i<archive.NumFiles;i++)if(renameMatch(archive.FileNames+2*archive.FileNameOffsets[i],"DIR",0))break;
 if(i==archive.NumFiles)valid=0;
 else{selectListItem((int)(i));deleteTesting=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);deleteTesting=FALSE;valid&=lastResult==SZ_OK&&opened&&archive.NumFiles==2;}
 fprintf(log,"%s Delete command removes selected folder subtree and verifies remaining data (result %d)\n",valid?"PASS":"FAIL",lastResult);
 if(valid){
  extractTesting=TRUE;strcpy(extractDestination,"C:\\G38OUT");SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);extractTesting=FALSE;
  valid&=lastResult==SZ_OK&&verifyExtractedFile("C:\\G38OUT\\KEEP.TXT",102400UL,0x9a0e0c8cUL)&&verifyExtractedFile("C:\\G38OUT\\C31IN.TXT",102400UL,0x9a0e0c8cUL);
  if(lastResult==SZ_OK&&(remove("C:\\G38OUT\\KEEP.TXT")||remove("C:\\G38OUT\\C31IN.TXT")||rmdir("C:\\G38OUT")))valid=0;
 }
 fprintf(log,"%s exact extraction of replaced and added content after deletion\n",valid?"PASS":"FAIL");
 closeArchive();if(windowAllocationCount)valid=0;if(remove("C:\\KEEP.TXT"))valid=0;
 return valid;
}
static int zipCreationSelfTest(FILE *log){
 int valid=1,method;FILE *empty;UInt32 crc,size;struct stat info;
 empty=fopen("C:\\C39ZERO.TXT","wb");if(!empty)return 0;if(fclose(empty))return 0;
 for(method=0;method<2;method++){
  createTesting=zipCreateTesting=TRUE;createZipMode=FALSE;createMethod=method;
  if(!setCreateSource("C:\\C31IN.TXT\r\nC:\\C39ZERO.TXT\r\n"))return 0;strcpy(createDestination,method?"C:\\G39DEF.ZIP":"C:\\G39COPY.ZIP");
  SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);valid&=lastResult==SZ_OK&&opened&&archive.NumFiles==2&&createZipMode;
  if(opened){performTest();valid&=lastResult==SZ_OK;}
  valid&=renameIdentity(createDestination,&crc,&size);
  if(method)valid&=size<102400UL;
  fprintf(log,"%s ZIP %s format selection and Create dialog; nonempty and empty files (result %d)\n",valid?"PASS":"FAIL",method?"Deflate":"Store",lastResult);
  extractTesting=TRUE;strcpy(extractDestination,method?"C:\\G39DEF":"C:\\G39COPY");SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);extractTesting=FALSE;
  if(lastResult==SZ_OK){char path[260];sprintf(path,"%s\\C31IN.TXT",extractDestination);valid&=verifyExtractedFile(path,102400UL,0x9a0e0c8cUL);if(remove(path))valid=0;sprintf(path,"%s\\C39ZERO.TXT",extractDestination);valid&=verifyExtractedFile(path,0,0);if(remove(path)||rmdir(extractDestination))valid=0;}else valid=0;
  fprintf(log,"%s ZIP %s exact extraction\n",valid?"PASS":"FAIL",method?"Deflate":"Store");
  SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);valid&=lastResult==SZ_ERROR_WRITE&&verifyExtractedFile(createDestination,size,crc);
  createCancelAfterRead=TRUE;strcpy(createDestination,"C:\\G39CANC.ZIP");SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);
  valid&=lastResult==SZ_ERROR_PROGRESS&&stat(createDestination,&info)!=0;createCancelAfterRead=FALSE;
  fprintf(log,"%s ZIP %s existing archive protection and cancellation cleanup\n",valid?"PASS":"FAIL",method?"Deflate":"Store");
  closeArchive();if(windowAllocationCount)valid=0;
 }
 createTesting=zipCreateTesting=createZipMode=FALSE;createMethod=1;if(remove("C:\\C39ZERO.TXT"))valid=0;return valid;
}
static int copyFixture(const char *from,const char *to){
 FILE *src=fopen(from,"rb"),*dst;Byte buffer[512];size_t n;int valid=1;
 if(!src)return 0;dst=fopen(to,"wb");if(!dst){fclose(src);return 0;}
 while((n=fread(buffer,1,sizeof(buffer),src))!=0)if(fwrite(buffer,1,n,dst)!=n){valid=0;break;}
 if(ferror(src))valid=0;if(fclose(dst))valid=0;fclose(src);return valid;
}
static int zipUpdateSelfTest(FILE *log){
 int valid=1,method;UInt32 crc,size;unsigned i;
 for(method=0;method<2;method++){
  char path[260];sprintf(path,"C:\\G41%s.ZIP",method?"DEF":"COPY");
  if(!copyFixture("C:\\ZUP41.ZIP",path)||!copyFixture("C:\\C31IN.TXT","C:\\DEFLATE.BIN"))return 0;
  SetWindowText(pathEdit,path);loadArchive();if(!opened)return 0;
  createTesting=TRUE;createMethod=method;if(!setCreateSource("C:\\DEFLATE.BIN\r\nC:\\C31IN.TXT\r\n"))return 0;
  SendMessage(mainWindow,WM_COMMAND,ID_UPDATE,0);valid&=lastResult==SZ_OK&&opened&&createZipMode&&archive.NumFiles==5;
  if(opened){performTest();valid&=lastResult==SZ_OK;}
  fprintf(log,"%s ZIP %s Update dialog replaces and adds content (result %d)\n",valid?"PASS":"FAIL",method?"Deflate":"Store",lastResult);
  for(i=0;i<archive.NumFiles;i++)if(renameMatch(archive.FileNames+2*archive.FileNameOffsets[i],"SUB",0))break;
  if(i==archive.NumFiles)valid=0;
  else{
   selectListItem((int)(i));renameTesting=TRUE;strcpy(renameNew,"MOVED");SendMessage(mainWindow,WM_COMMAND,ID_RENAME,0);
   valid&=lastResult==SZ_OK&&opened&&renameMatch(archive.FileNames+2*archive.FileNameOffsets[i],"MOVED",0);
   valid&=renameIdentity(path,&crc,&size);renameCancelAfterWrite=TRUE;strcpy(renameNew,"CANCEL");SendMessage(mainWindow,WM_COMMAND,ID_RENAME,0);
   valid&=lastResult==SZ_ERROR_PROGRESS&&verifyExtractedFile(path,size,crc);renameTesting=renameCancelAfterWrite=FALSE;
  }
  fprintf(log,"%s ZIP Rename dialog updates folder descendants and cancellation preserves original\n",valid?"PASS":"FAIL");
  valid&=renameIdentity(path,&crc,&size);createCancelAfterRead=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_UPDATE,0);
  valid&=lastResult==SZ_ERROR_PROGRESS&&verifyExtractedFile(path,size,crc);createCancelAfterRead=FALSE;createTesting=FALSE;
  fprintf(log,"%s cancelled ZIP update preserves original bytes\n",valid?"PASS":"FAIL");
  for(i=0;i<archive.NumFiles;i++)if(renameMatch(archive.FileNames+2*archive.FileNameOffsets[i],"MOVED",0))break;
  if(i==archive.NumFiles)valid=0;
  else{selectListItem((int)(i));deleteTesting=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);deleteTesting=FALSE;valid&=lastResult==SZ_OK&&opened&&archive.NumFiles==3;}
  fprintf(log,"%s ZIP Delete command removes the selected folder and descendants\n",valid?"PASS":"FAIL");
  if(valid){
   extractTesting=TRUE;strcpy(extractDestination,"C:\\G41OUT");SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);extractTesting=FALSE;
   valid&=lastResult==SZ_OK&&verifyExtractedFile("C:\\G41OUT\\DEFLATE.BIN",102400UL,0x9a0e0c8cUL)&&verifyExtractedFile("C:\\G41OUT\\C31IN.TXT",102400UL,0x9a0e0c8cUL);
   if(lastResult==SZ_OK&&(remove("C:\\G41OUT\\DEFLATE.BIN")||remove("C:\\G41OUT\\C31IN.TXT")||rmdir("C:\\G41OUT\\EMPTY")||rmdir("C:\\G41OUT")))valid=0;
  }
  fprintf(log,"%s exact ZIP replacement/addition extraction after deletion\n",valid?"PASS":"FAIL");
  closeArchive();if(windowAllocationCount)valid=0;if(remove("C:\\DEFLATE.BIN"))valid=0;
 }
 return valid;
}
static int flatExtractionSelfTest(FILE *log){
 int valid=1,which;struct stat info;
 for(which=0;which<2;which++){
  SetWindowText(pathEdit,which?"C:\\WIN16.ZIP":"C:\\EXTRACT.7Z");loadArchive();if(!opened)return 0;
  extractTesting=extractFlat=TRUE;strcpy(extractDestination,"C:\\G44FLAT");
  SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);valid&=lastResult==SZ_OK&&extractFlat&&strstr(extractMessage,"directory metadata (paths were omitted)")!=NULL;
  if(lastResult==SZ_OK){
   if(which){valid&=verifyExtractedFile("C:\\G44FLAT\\DEFLATE.BIN",306000UL,0x2783538aUL)&&verifyExtractedFile("C:\\G44FLAT\\ZERO.TXT",0,0);}
   else{valid&=verifyExtractedFile("C:\\G44FLAT\\FIRST.TXT",240000UL,0x4bd252cbUL)&&verifyExtractedFile("C:\\G44FLAT\\SECOND.BIN",102400UL,0x9a0e0c8cUL)&&verifyExtractedFile("C:\\G44FLAT\\ZERO.TXT",0,0);}
   valid&=stat("C:\\G44FLAT\\SUB",&info)!=0&&stat("C:\\G44FLAT\\EMPTY",&info)!=0;
   SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);valid&=lastResult==SZ_ERROR_WRITE;
   if(remove("C:\\G44FLAT\\ZERO.TXT"))valid=0;
   if(which){if(remove("C:\\G44FLAT\\DEFLATE.BIN"))valid=0;}
   else if(remove("C:\\G44FLAT\\FIRST.TXT")||remove("C:\\G44FLAT\\SECOND.BIN"))valid=0;
   if(rmdir("C:\\G44FLAT"))valid=0;
  }
  fprintf(log,"%s %s pathless Extract dialog, exact contents, metadata warning and existing output protection\n",valid?"PASS":"FAIL",which?"ZIP":"7z");
  extractCancelTesting=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);extractCancelTesting=FALSE;
  valid&=lastResult==SZ_ERROR_PROGRESS&&stat(extractDestination,&info)!=0;
  fprintf(log,"%s pathless extraction cancellation leaves no output\n",valid?"PASS":"FAIL");
 }
 SetWindowText(pathEdit,"C:\\FLATBAD.ZIP");loadArchive();if(!opened)valid=0;
 SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);valid&=lastResult==SZ_ERROR_UNSUPPORTED&&stat(extractDestination,&info)!=0;
 fprintf(log,"%s colliding basenames rejected before output\n",valid?"PASS":"FAIL");
 extractFlat=extractTesting=FALSE;closeArchive();if(windowAllocationCount)valid=0;return valid;
}
static int treeSelfTest(FILE *log){
 int valid=1,which;struct stat info;UInt32 beforeCRC,afterCRC;unsigned long beforeSize,afterSize;
 const char *output[4]={"C:\\G46COPY.7Z","C:\\G46LZMA.7Z","C:\\G46STORE.ZIP","C:\\G46DEFL.ZIP"};
 for(which=0;which<4;which++){
  createTesting=TRUE;createZipMode=which>=2;createMethod=which&1;
  strcpy(createDestination,which>=2?"C:\\TREE\\SELF.ZIP":"C:\\TREE\\SELF.7Z");if(!setCreateSource("C:\\TREE\r\n"))return 0;
  SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);
  valid&=lastResult==SZ_OK&&opened&&archive.NumFiles==7;
  fprintf(log,"%s recursive Create dialog format=%s method=%d entries=%lu result=%d\n",valid?"PASS":"FAIL",which>=2?"ZIP":"7z",createMethod,opened?archive.NumFiles:0UL,lastResult);
  if(!valid)break;
  if(mkdir("C:\\TREE\\NEW")||!copyFixture("C:\\C31IN.TXT","C:\\TREE\\NEW\\ADD.TXT")||!copyFixture("C:\\TREE\\SUB\\B.BIN","C:\\TREE\\A.TXT"))return 0;
  SendMessage(mainWindow,WM_COMMAND,ID_UPDATE,0);
  valid&=lastResult==SZ_OK&&opened&&archive.NumFiles==9;
  fprintf(log,"%s recursive Update dialog, nested addition/replacement and archive self exclusion; result=%d\n",valid?"PASS":"FAIL",lastResult);
  extractTesting=TRUE;extractFlat=FALSE;strcpy(extractDestination,"C:\\G46OUT");SendMessage(mainWindow,WM_COMMAND,ID_EXTRACT,0);
  valid&=lastResult==SZ_OK&&verifyExtractedFile("C:\\G46OUT\\TREE\\A.TXT",240000UL,0x4bd252cbUL)&&verifyExtractedFile("C:\\G46OUT\\TREE\\SUB\\B.BIN",240000UL,0x4bd252cbUL)&&verifyExtractedFile("C:\\G46OUT\\TREE\\NEW\\ADD.TXT",102400UL,0x9a0e0c8cUL)&&verifyExtractedFile("C:\\G46OUT\\TREE\\SUB\\DEEP\\ZERO.TXT",0,0);
  valid&=!stat("C:\\G46OUT\\TREE\\EMPTY",&info)&&(info.st_mode&S_IFDIR);
  fprintf(log,"%s exact extracted tree, nested data and empty directory\n",valid?"PASS":"FAIL");
  if(!valid)break;
  if(remove("C:\\G46OUT\\TREE\\A.TXT")||remove("C:\\G46OUT\\TREE\\SUB\\B.BIN")||remove("C:\\G46OUT\\TREE\\NEW\\ADD.TXT")||remove("C:\\G46OUT\\TREE\\SUB\\DEEP\\ZERO.TXT")||rmdir("C:\\G46OUT\\TREE\\SUB\\DEEP")||rmdir("C:\\G46OUT\\TREE\\SUB")||rmdir("C:\\G46OUT\\TREE\\EMPTY")||rmdir("C:\\G46OUT\\TREE\\NEW")||rmdir("C:\\G46OUT\\TREE")||rmdir("C:\\G46OUT"))return 0;
  valid&=renameIdentity(createDestination,&beforeCRC,&beforeSize);createCancelTesting=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_UPDATE,0);createCancelTesting=FALSE;
  valid&=lastResult==SZ_ERROR_PROGRESS&&renameIdentity(createDestination,&afterCRC,&afterSize)&&beforeCRC==afterCRC&&beforeSize==afterSize;
  fprintf(log,"%s cancelled recursive update preserves original archive\n",valid?"PASS":"FAIL");
  closeArchive();if(windowAllocationCount)valid=0;
  if(!copyFixture(createDestination,output[which])||remove(createDestination)||remove("C:\\TREE\\NEW\\ADD.TXT")||rmdir("C:\\TREE\\NEW")||!copyFixture("C:\\C31IN.TXT","C:\\TREE\\A.TXT"))return 0;
 }
 createTesting=extractTesting=createZipMode=FALSE;return valid;
}
static int associationSelfTest(FILE *log){
 char old7z[260],oldZip[260],current[260];int has7z,hasZip,has,valid=1,applied;HWND child=0;DWORD start;MSG msg;char launch[520],seenPath[260];long seenCount=-1;unsigned started,seen=0;
 if(GetProfileString(ASSOC_SECTION,NULL,"",current,sizeof(current))||!assocRead("Extensions","7z",old7z,&has7z)||!assocRead("Extensions","zip",oldZip,&hasZip))return 0;
 if(!WriteProfileString("Extensions","7z","OLDAPP.EXE ^.7z")||!WriteProfileString("Extensions","zip",NULL))return 0;
 associationTesting=TRUE;associationChoices=3;
 applied=showAssociations(mainWindow);fprintf(log,"Association dialog result=%d 7z=%d zip=%d\n",applied,assocCurrent(associationExecutable,"7z"),assocCurrent(associationExecutable,"zip"));
 valid&=applied==1&&assocCurrent(associationExecutable,"7z")&&assocCurrent(associationExecutable,"zip");
 fprintf(log,"%s File associations dialog enables 7z and ZIP with saved previous values\n",valid?"PASS":"FAIL");
 /* The same command line that File Manager constructs must open the archive. */
 sprintf(launch,"%s \"C:\\WIN16.ZIP\"",associationExecutable);started=WinExec(launch,SW_SHOWNORMAL);valid&=started>=32;start=GetTickCount();seenPath[0]=0;
 while(valid&&GetTickCount()-start<15000UL){HWND w=GetWindow(mainWindow,GW_HWNDFIRST);
  while(w){if(w!=mainWindow){char cls[40];GetClassName(w,cls,sizeof(cls));if(!stricmp(cls,"Z7Win16Browser")){SendMessage(GetDlgItem(w,110),WM_GETTEXT,sizeof(current),(LPARAM)(LPSTR)current);seen=(unsigned)w;strcpy(seenPath,current);seenCount=SendMessage(GetDlgItem(w,111),LB_GETCOUNT,0,0);if(!stricmp(current,"C:\\WIN16.ZIP")&&seenCount==3){child=w;break;}}}w=GetWindow(w,GW_HWNDNEXT);}
  if(child)break;
  while(PeekMessage(&msg,0,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}Yield();
 }
 fprintf(log,"Launch result=%u last matching window=%u path=%s count=%ld elapsed=%lu\n",started,seen,seenPath,seenCount,GetTickCount()-start);
 valid&=child!=0;if(child)SendMessage(child,WM_CLOSE,0,0);
 fprintf(log,"%s associated command opens a quoted archive path in a separate GUI instance\n",valid?"PASS":"FAIL");
 WriteProfileString("Extensions","zip","NEWAPP.EXE ^.zip");associationChoices=0;
 valid&=showAssociations(mainWindow)==1;
 valid&=assocRead("Extensions","7z",current,&has)&&has&&!strcmp(current,"OLDAPP.EXE ^.7z");
 valid&=assocRead("Extensions","zip",current,&has)&&has&&!strcmp(current,"NEWAPP.EXE ^.zip");
 valid&=GetProfileString(ASSOC_SECTION,NULL,"",current,sizeof(current))==0;
 fprintf(log,"%s disable restores previous association and preserves a later user change\n",valid?"PASS":"FAIL");
 associationTesting=FALSE;
 if(!WriteProfileString("Extensions","7z",has7z?old7z:NULL)||!WriteProfileString("Extensions","zip",hasZip?oldZip:NULL))valid=0;
 return valid;
}
static int dictionarySelfTest(FILE *log){
 const char *paths[4]={"C:\\LZ128.7Z","C:\\LZ32M.7Z","C:\\L2128.7Z","C:\\L232M.7Z"};
 unsigned which;int valid=1;struct stat info;
 for(which=0;which<4;which++){
  SetWindowText(pathEdit,paths[which]);loadArchive();valid&=opened&&lastResult==SZ_OK;
  if(!opened)break;
  performTest();valid&=lastResult==SZ_OK;
  fprintf(log,"%s paged dictionary integrity %s result=%d\n",valid?"PASS":"FAIL",paths[which],lastResult);fflush(log);
  extractTesting=TRUE;extractFlat=FALSE;strcpy(extractDestination,"C:\\P53OUT");
  performExtract();valid&=lastResult==SZ_OK;
  if(lastResult==SZ_OK){valid&=verifyExtractedFile("C:\\P53OUT\\LARGE.BIN",360000UL,0x354904edUL);if(remove("C:\\P53OUT\\LARGE.BIN")||rmdir(extractDestination))valid=0;}
  fprintf(log,"%s paged dictionary exact extraction %s result=%d\n",valid?"PASS":"FAIL",paths[which],lastResult);fflush(log);
  closeArchive();valid&=!windowAllocationCount;
 }
 SetWindowText(pathEdit,"C:\\L232M.7Z");loadArchive();dictionaryCancelAfter=20000;
 performExtract();valid&=lastResult==SZ_ERROR_PROGRESS&&stat(extractDestination,&info)!=0&&!dictionaryCancelAfter;
 fprintf(log,"%s cancellation after dictionary eviction removes partial extraction; result=%d\n",valid?"PASS":"FAIL",lastResult);fflush(log);
 performTest();valid&=lastResult==SZ_OK;closeArchive();valid&=!windowAllocationCount;
 SetWindowText(pathEdit,"C:\\L2BAD.7Z");loadArchive();performExtract();
 valid&=lastResult==SZ_ERROR_CRC&&stat(extractDestination,&info)!=0;
 fprintf(log,"%s paged dictionary CRC failure removes partial extraction; result=%d\n",valid?"PASS":"FAIL",lastResult);fflush(log);
 closeArchive();valid&=!windowAllocationCount;extractTesting=FALSE;
 return valid;
}
static int filterSelfTest(FILE *log){
 static const char *linearNames[]={"FBCJ","FARM","FARMT","FARM64","FPPC","FSPARC","FIA64","FRISCV","FDELTA","FA64PC","FRVCPC","FCHAIN2","FCHAIN3","FCOPY","FDFLT"};
 static const char *bcjNames[]={"BRAW","BLZMA","BLZMA2","BDFLT"};
 const char **names=bcjOnlyTest?bcjNames:linearNames;
 unsigned count=bcjOnlyTest?4:sizeof(linearNames)/sizeof(linearNames[0]);
 unsigned which;int valid=1;char path[40];struct stat info;
 extractTesting=TRUE;extractFlat=FALSE;strcpy(extractDestination,"C:\\F59OUT");
 for(which=0;which<count;which++){
  sprintf(path,"C:\\%s.7Z",names[which]);SetWindowText(pathEdit,path);loadArchive();valid&=opened&&lastResult==SZ_OK;
  if(!opened)break;
  performTest();valid&=lastResult==SZ_OK;
  fprintf(log,"%s streaming filter integrity %s result=%d\n",valid?"PASS":"FAIL",names[which],lastResult);fflush(log);
  performExtract();valid&=lastResult==SZ_OK;
  if(lastResult==SZ_OK){valid&=verifyExtractedFile("C:\\F59OUT\\LARGE.BIN",360003UL,(bcjOnlyTest&&archive.CRCs.Vals[0]==0x37a23bbfUL)?0x37a23bbfUL:0x65f37791UL);if(remove("C:\\F59OUT\\LARGE.BIN")||rmdir(extractDestination))valid=0;}
  fprintf(log,"%s streaming filter exact extraction %s result=%d\n",valid?"PASS":"FAIL",names[which],lastResult);fflush(log);
  closeArchive();valid&=!windowAllocationCount;
 }
 SetWindowText(pathEdit,bcjOnlyTest?"C:\\BLZMA2.7Z":"C:\\FCHAIN3.7Z");loadArchive();dictionaryCancelAfter=20000;
 performExtract();valid&=lastResult==SZ_ERROR_PROGRESS&&stat(extractDestination,&info)!=0&&!dictionaryCancelAfter;
 fprintf(log,"%s filter-chain cancellation removes partial output and dictionary backing file; result=%d\n",valid?"PASS":"FAIL",lastResult);fflush(log);
 performTest();valid&=lastResult==SZ_OK;closeArchive();valid&=!windowAllocationCount;
 SetWindowText(pathEdit,bcjOnlyTest?"C:\\BBAD.7Z":"C:\\FBAD.7Z");loadArchive();performExtract();
 valid&=lastResult==SZ_ERROR_CRC&&stat(extractDestination,&info)!=0;
 fprintf(log,"%s filtered output CRC failure removes partial extraction; result=%d\n",valid?"PASS":"FAIL",lastResult);fflush(log);
 closeArchive();valid&=!windowAllocationCount;
 if(bcjOnlyTest){
  unsigned baseline;int real=!(GetWinFlags()&WF_PMODE),passed;
  SetWindowText(pathEdit,"C:\\BRAW.7Z");loadArchive();baseline=windowAllocationCount;
  if(!opened)valid=0;
  else{
   windowAllocationFailures=0;windowAllocationFailuresRemaining=1;
   performExtract();windowAllocationFailuresRemaining=0;
   passed=lastResult==(real?SZ_OK:SZ_ERROR_MEM)&&windowAllocationFailures==1&&windowAllocationCount==baseline;
   if(real&&lastResult==SZ_OK){
    passed&=verifyExtractedFile("C:\\F59OUT\\LARGE.BIN",360003UL,archive.CRCs.Vals[0]);
    if(remove("C:\\F59OUT\\LARGE.BIN")||rmdir(extractDestination))passed=0;
   }else passed&=stat(extractDestination,&info)!=0;
   valid&=passed;
   fprintf(log,"%s injected allocation failure: real-mode recovery or protected-mode error; result=%d failures=%u\n",passed?"PASS":"FAIL",lastResult,windowAllocationFailures);fflush(log);
   windowAllocationFailures=0;windowAllocationFailuresRemaining=2;
   performExtract();windowAllocationFailuresRemaining=0;
   passed=lastResult==SZ_ERROR_MEM&&windowAllocationFailures==(real?2:1)&&windowAllocationCount==baseline&&stat(extractDestination,&info)!=0;
   valid&=passed;
   fprintf(log,"%s persistent allocation failure: bounded attempts and no partial extraction; result=%d failures=%u\n",passed?"PASS":"FAIL",lastResult,windowAllocationFailures);fflush(log);
  }
  closeArchive();valid&=!windowAllocationCount;
 }
 extractTesting=FALSE;
 return valid;
}
static int hashSelfTest(FILE *log){
 static const char *expected[Z7_WIN16_HASH_COUNT]={"352441C2","2CD8094A1A277627","A9993E364706816ABA3E25717850C26C9CD0D89D","BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD","CB00753F45A35E8BB5A03D699AC65007272C32AB0EDED1631A8B605A43FF5BED8086072BA1E7CC2358BAECA134C825A7","DDAF35A193617ABACC417349AE20413112E6FA4E89A97EA20A9EEEE64B55D39A2192992A274FC1A836BA3C23A3FEEBBD454D4423643CE80E2A9AC94FA54CA49F","3A985DA74FE225B2045C172D6BD390BD855F086E3E9D525B46BFE24511431532","900150983CD24FB0D6963F7D28E17F72","70F75B58F1FECAB821DB43C88AD84EDDE5A52600616CD22517B7BB14D440A7D5","44BC2CF5AD770999","48AA78A2"};
 static const char *expectedBig[Z7_WIN16_HASH_COUNT]={"EF4188C8","5C0A51FC2F169D2C","B387A04C3BEA4D061057DE558E767362FB17F29A","F459F648C95BEF4A0D03ED5D36CD902BD1CB195ECF8BDF5BE019CA1FB7546962","FCCD739ED97417B5CD97BD4834146849458EE4B4FDB35C827545E43559C5FA9F478A1171DB71D8BE40D43BF00A526EDD","246E6A57B4DD3D5567C1D216B13B522348B4EAB56BCB9291BB46B75A84D20FF0E452698AAD0FD2DAC50C63360595690119E7178AEE15D1AFA5E21EDF3B5B0C57","B90C28596E6E38CC45046C3BD66578893EBF1B4495A38E3B37269265ADDEAEAB","A87FB076C5AE1C8CF79ADD24F70A4880","D6BB7BF1E0C09F29338F244C59CDD1FA6D28BA4584D0CC5D4BEEE8F55074197D","B94F2D7AEB6A4AD0","7E2780E7"};
 unsigned i,baseline=windowAllocationCount;int valid=1;HGLOBAL text;char FAR *value;
 hashTesting=TRUE;hashChoice=0;strcpy(hashSource,"C:\\H49ABC.TXT");SendMessage(mainWindow,WM_COMMAND,ID_HASH,0);
 fprintf(log,"ABC checksum result=%d blocks=%u\n",lastResult,windowAllocationCount);
 valid&=lastResult==SZ_OK;for(i=0;i<Z7_WIN16_HASH_COUNT;i++)if(!strstr(hashResults,expected[i])){fprintf(log,"Missing abc digest for %s\n",Z7Win16_HashMethods[i].name);valid=0;}
 if(OpenClipboard(mainWindow)){text=(HGLOBAL)GetClipboardData(CF_TEXT);value=text?(char FAR *)GlobalLock(text):NULL;valid&=value&&!strcmp(value,hashResults);if(value)GlobalUnlock(text);CloseClipboard();}else valid=0;
 fprintf(log,"%s all 11 file checksums through the GUI and Copy results clipboard button\n",valid?"PASS":"FAIL");
 hashChoice=0;strcpy(hashSource,"C:\\H49BIG.BIN");SendMessage(mainWindow,WM_COMMAND,ID_HASH,0);
 fprintf(log,"Large checksum result=%d blocks=%u\n",lastResult,windowAllocationCount);
 valid&=lastResult==SZ_OK;for(i=0;i<Z7_WIN16_HASH_COUNT;i++)if(!strstr(hashResults,expectedBig[i])){fprintf(log,"Missing digest for %s\n",Z7Win16_HashMethods[i].name);valid=0;}
#ifdef Z7_HASH_TRACE
 fputs(hashResults,log);fputc('\n',log);
#endif
 fprintf(log,"%s all 11 checksums of a 131071-byte file using streaming reads\n",valid?"PASS":"FAIL");
 hashChoice=0;hashCancelAfterRead=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_HASH,0);
 fprintf(log,"Checksum cancellation result=%d remaining=%u baseline=%u pending=%d resultsEmpty=%d\n",lastResult,windowAllocationCount,baseline,hashCancelAfterRead,!hashResults[0]);
 valid&=lastResult==SZ_ERROR_PROGRESS&&!hashResults[0]&&windowAllocationCount==baseline;
 fprintf(log,"%s checksum cancellation removes partial results and frees working memory\n",valid?"PASS":"FAIL");
 hashChoice=4;strcpy(hashSource,"C:\\H49EMPTY.TXT");SendMessage(mainWindow,WM_COMMAND,ID_HASH,0);
 valid&=lastResult==SZ_OK&&strstr(hashResults,"E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855")!=NULL;
 strcpy(hashSource,"C:\\H49NONE.TXT");SendMessage(mainWindow,WM_COMMAND,ID_HASH,0);
 valid&=lastResult==SZ_ERROR_READ&&!hashResults[0]&&windowAllocationCount==baseline;
 fprintf(log,"%s empty input and missing-file error; no stale successful results\n",valid?"PASS":"FAIL");
 hashTesting=FALSE;
 /* Loading checksum code must not make the real-mode encoder unusable. */
 createTesting=TRUE;createZipMode=FALSE;createMethod=1;if(!setCreateSource("C:\\H49BIG.BIN"))return 0;strcpy(createDestination,"C:\\G49AFTER.7Z");
 SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);
 fprintf(log,"Post-checksum create result=%d opened=%d blocks=%u\n",lastResult,opened,windowAllocationCount);
 valid&=lastResult==SZ_OK&&opened&&archive.NumFiles==1&&SzArEx_GetFileSize(&archive,0)==131071UL;
 if(opened){performTest();valid&=lastResult==SZ_OK;}closeArchive();
 if(remove(createDestination))valid=0;createTesting=FALSE;
 fprintf(log,"%s LZMA creation and integrity test after loading all checksum algorithms\n",valid?"PASS":"FAIL");
 return valid;
}
#ifdef __WATCOMC__
#pragma code_seg("HASH_TEST_TEXT")
#endif
static int hashRecoverySelfTest(FILE *log){
 unsigned baseline=windowAllocationCount;int valid=1,passed,real=!(GetWinFlags()&WF_PMODE);
 hashTesting=TRUE;hashChoice=0;strcpy(hashSource,"C:\\H49ABC.TXT");
 windowAllocationFailures=0;windowAllocationFailuresRemaining=1;
 SendMessage(mainWindow,WM_COMMAND,ID_HASH,0);windowAllocationFailuresRemaining=0;
 passed=lastResult==(real?SZ_OK:SZ_ERROR_MEM)&&windowAllocationFailures==1&&windowAllocationCount==baseline;
 if(real)passed&=strstr(hashResults,"352441C2")!=NULL&&strstr(hashResults,"BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD")!=NULL;
 else passed&=!hashResults[0];
 valid&=passed;fprintf(log,"%s checksum allocation recovery: result=%d failures=%u blocks=%u\n",passed?"PASS":"FAIL",lastResult,windowAllocationFailures,windowAllocationCount);
 windowAllocationFailures=0;windowAllocationFailuresRemaining=2;
 SendMessage(mainWindow,WM_COMMAND,ID_HASH,0);windowAllocationFailuresRemaining=0;
 passed=lastResult==SZ_ERROR_MEM&&windowAllocationFailures==(real?2:1)&&windowAllocationCount==baseline&&!hashResults[0];
 valid&=passed;fprintf(log,"%s persistent checksum allocation failure: result=%d failures=%u blocks=%u\n",passed?"PASS":"FAIL",lastResult,windowAllocationFailures,windowAllocationCount);
 if(real){
  windowAllocationFailures=0;windowAllocationFailuresRemaining=2;windowAllocationCancelOnFailure=TRUE;
  SendMessage(mainWindow,WM_COMMAND,ID_HASH,0);windowAllocationFailuresRemaining=0;windowAllocationCancelOnFailure=FALSE;
  passed=lastResult==SZ_ERROR_PROGRESS&&windowAllocationFailures==1&&windowAllocationCount==baseline&&!hashResults[0];
  valid&=passed;fprintf(log,"%s cancellation before checksum retry: result=%d failures=%u blocks=%u\n",passed?"PASS":"FAIL",lastResult,windowAllocationFailures,windowAllocationCount);
 }
 hashTesting=FALSE;return valid;
}
#ifdef __WATCOMC__
#pragma code_seg("FS_TEST_TEXT")
#endif
static int directoryTestRow(const char *wanted){
 long count=SendMessage(fileList,LB_GETCOUNT,0,0),i;char line[80],*tab;
 for(i=0;i<count;i++){
  long length=SendMessage(fileList,LB_GETTEXTLEN,(WPARAM)i,0);
  if(length<0||length>=sizeof(line))continue;
  SendMessage(fileList,LB_GETTEXT,(WPARAM)i,(LPARAM)(LPSTR)line);tab=strchr(line,'\t');if(tab)*tab=0;
  if(!stricmp(line,wanted))return (int)i;
 }
 return -1;
}
#ifdef __WATCOMC__
static unsigned filesystemTestCs(void);
#pragma aux filesystemTestCs = "mov ax,cs" value [ax] modify exact [ax];
#endif
static int browseSelfTestBody(FILE *log){
 FILE *source,*target;char buffer[512],text[260];size_t size;struct stat info;int valid=1,row;unsigned i;
 if(!stat("C:\\B63VIEW",&info)){fputs("FAIL browser fixture already exists\n",log);return 0;}
 if(mkdir("C:\\B63VIEW")||mkdir("C:\\B63VIEW\\SUB")){fputs("FAIL browser fixture directories\n",log);return 0;}
 source=fopen("C:\\WIN16.ZIP","rb");target=fopen("C:\\B63VIEW\\TEST.ZIP","wb");
 if(!source||!target){if(source)fclose(source);if(target)fclose(target);return 0;}
 while((size=fread(buffer,1,sizeof(buffer),source))!=0)if(fwrite(buffer,1,size,target)!=size){valid=0;break;}
 if(ferror(source))valid=0;if(fclose(source))valid=0;if(fclose(target))valid=0;
 target=fopen("C:\\B63VIEW\\A.TXT","wb");if(!target)return 0;valid&=fwrite("abc",1,3,target)==3;valid&=fclose(target)==0;
 target=fopen("C:\\B63VIEW\\EMPTY.TXT","wb");if(!target)return 0;valid&=fclose(target)==0;
 target=fopen("C:\\B63VIEW\\HIDDEN.BIN","wb");if(!target)return 0;valid&=fwrite("hello",1,5,target)==5;valid&=fclose(target)==0;
 valid&=_dos_setfileattr("C:\\B63VIEW\\HIDDEN.BIN",_A_RDONLY|_A_HIDDEN|_A_SYSTEM)==0;
 SetWindowText(pathEdit,"C:\\B63VIEW\\");SendMessage(mainWindow,WM_COMMAND,ID_OPEN,0);
 valid&=lastResult==SZ_OK&&folderView&&!opened&&SendMessage(fileList,LB_GETCOUNT,0,0)==6&&sameName(folderPath,"C:\\B63VIEW");
 valid&=directoryTestRow("..")==0&&directoryTestRow("SUB")==1&&directoryTestRow("HIDDEN.BIN")>=0;
 row=directoryTestRow("A.TXT");if(row<0)valid=0;else{SendMessage(fileList,LB_GETTEXT,row,(LPARAM)(LPSTR)text);valid&=!strncmp(text,"A.TXT\t3\t",8);}
 row=directoryTestRow("EMPTY.TXT");if(row<0)valid=0;else{SendMessage(fileList,LB_GETTEXT,row,(LPARAM)(LPSTR)text);valid&=!strncmp(text,"EMPTY.TXT\t0\t",12);}
 fprintf(log,"%s directory list: parent, folders first, hidden/system/read-only files, zero and nonzero sizes\n",valid?"PASS":"FAIL");
 row=directoryTestRow("SUB");selectListItem((int)(row));SendMessage(mainWindow,WM_COMMAND,111,MAKELONG((unsigned)fileList,LBN_DBLCLK));
 valid&=folderView&&sameName(folderPath,"C:\\B63VIEW\\SUB")&&SendMessage(fileList,LB_GETCOUNT,0,0)==1;
 selectListItem((int)(0));SendMessage(mainWindow,WM_COMMAND,111,MAKELONG((unsigned)fileList,LBN_DBLCLK));
 valid&=folderView&&sameName(folderPath,"C:\\B63VIEW")&&SendMessage(fileList,LB_GETCOUNT,0,0)==6;
 fprintf(log,"%s native list double-click opens a folder and its parent\n",valid?"PASS":"FAIL");
 row=directoryTestRow("TEST.ZIP");selectListItem((int)(row));SendMessage(mainWindow,WM_COMMAND,111,MAKELONG((unsigned)fileList,LBN_DBLCLK));
 valid&=opened&&!folderView&&lastResult==SZ_OK&&archive.NumFiles==3;if(opened){performTest();valid&=lastResult==SZ_OK;}
 fprintf(log,"Browser archive integrity result=%d opened=%d files=%lu blocks=%u\n",lastResult,opened,opened?archive.NumFiles:0UL,windowAllocationCount);fflush(log);
 SendMessage(mainWindow,WM_COMMAND,ID_PARENT,0);valid&=folderView&&!opened&&sameName(folderPath,"C:\\B63VIEW")&&SendMessage(fileList,LB_GETCOUNT,0,0)==6;
 fprintf(log,"%s selected archive opens and tests; Up returns to its containing folder\n",valid?"PASS":"FAIL");
 SetWindowText(pathEdit,"C:\\");SendMessage(mainWindow,WM_COMMAND,ID_OPEN,0);valid&=folderView&&folderRoot(folderPath)&&directoryTestRow("..")<0;
 SendMessage(mainWindow,WM_COMMAND,ID_PARENT,0);valid&=folderView&&folderRoot(folderPath);
 SetWindowText(pathEdit,"C:\\B63MISS");SendMessage(mainWindow,WM_COMMAND,ID_OPEN,0);valid&=!opened&&!folderView&&lastResult!=SZ_OK;
 SetWindowText(pathEdit,"C:\\B63VIEW");for(i=0;i<32;i++){SendMessage(mainWindow,WM_COMMAND,ID_REFRESH,0);valid&=folderView&&lastResult==SZ_OK&&SendMessage(fileList,LB_GETCOUNT,0,0)==6&&!windowAllocationCount;}
 fprintf(log,"%s drive root, missing path recovery and 32 directory refreshes\n",valid?"PASS":"FAIL");
 closeArchive();
 valid&=_dos_setfileattr("C:\\B63VIEW\\HIDDEN.BIN",0)==0;
 valid&=remove("C:\\B63VIEW\\HIDDEN.BIN")==0;valid&=remove("C:\\B63VIEW\\EMPTY.TXT")==0;valid&=remove("C:\\B63VIEW\\A.TXT")==0;valid&=remove("C:\\B63VIEW\\TEST.ZIP")==0;valid&=rmdir("C:\\B63VIEW\\SUB")==0;valid&=rmdir("C:\\B63VIEW")==0;
 fprintf(log,"%s browser fixture cleanup and no open archive allocations\n",valid&&!windowAllocationCount?"PASS":"FAIL");return valid&&!windowAllocationCount;
}
static int browseSelfTest(FILE *log){
 int valid;
#ifdef __WATCOMC__
 unsigned segment=filesystemTestCs();
 if(!LockSegment(segment)){fputs("FAIL cannot retain browser test code\n",log);return 0;}
#endif
 valid=browseSelfTestBody(log);
#ifdef __WATCOMC__
 UnlockSegment(segment);
#endif
 return valid;
}
#ifdef __WATCOMC__
#pragma code_seg("GUI_TEST_TEXT")
#endif
#ifdef __WATCOMC__
#pragma code_seg("BATCH_TEST_TEXT")
#endif
static int selectTwoFilesystemItems(const char *first,const char *second){
 int a=directoryTestRow(first),b=directoryTestRow(second);
 if(!fileListMultiple||a<0||b<0||a==b)return 0;
 selectListItem(-1);SendMessage(fileList,LB_SETSEL,TRUE,a);SendMessage(fileList,LB_SETSEL,TRUE,b);
 SendMessage(mainWindow,WM_COMMAND,111,MAKELONG((unsigned)fileList,LBN_SELCHANGE));
 return SendMessage(fileList,LB_GETSELCOUNT,0,0)==2;
}
static int batchFilesystemSelfTestBody(FILE *log){
 int valid=1,i;FILE *file;struct stat info;char parent[260],guard[260],*slash;unsigned baseline=windowAllocationCount;
 if(!stat("C:\\F79VIEW",&info)||mkdir("C:\\F79VIEW")){fputs("FAIL private batch fixture must be new\n",log);return 0;}
 file=fopen("C:\\F79VIEW\\A.BIN","wb");if(!file)return 0;for(i=0;i<8193;i++)if(fputc(i&255,file)==EOF)valid=0;if(fclose(file))valid=0;
 file=fopen("C:\\F79VIEW\\B.BIN","wb");if(!file)return 0;for(i=0;i<12291;i++)if(fputc(((unsigned)i*17u+3u)&255u,file)==EOF)valid=0;if(fclose(file))valid=0;
 valid&=!mkdir("C:\\F79VIEW\\OUT1")&&!mkdir("C:\\F79VIEW\\OUT2");loadDirectory("C:\\F79VIEW");
 valid&=selectTwoFilesystemItems("..","A.BIN");strcpy(fsDestination,"OUT1");SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);
 valid&=lastResult==SZ_ERROR_PARAM&&stat("C:\\F79VIEW\\OUT1\\A.BIN",&info)!=0;
 valid&=selectTwoFilesystemItems("A.BIN","B.BIN");windowAllocationFailureSize=2*(sizeof(int)+13);windowAllocationFailuresRemaining=1;windowAllocationFailures=0;
 SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);valid&=lastResult==SZ_ERROR_MEM&&windowAllocationFailures==1&&windowAllocationCount==baseline&&stat("C:\\F79VIEW\\OUT1\\A.BIN",&info)!=0;
 windowAllocationFailureSize=windowAllocationFailuresRemaining=0;
 fprintf(log,"%s multiple selection rejects parent and allocation failure before changes\n",valid?"PASS":"FAIL");fflush(log);
 SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);
 valid&=lastResult==SZ_OK&&fsBatchCompleted==2&&!fsBatchSkipped&&verifyExtractedFile("C:\\F79VIEW\\OUT1\\A.BIN",8193,0x4cd01d7dUL)&&verifyExtractedFile("C:\\F79VIEW\\OUT1\\B.BIN",12291,0xba8ed439UL);
 file=fopen("C:\\F79VIEW\\OUT1\\A.BIN","wb");if(!file)return 0;valid&=fwrite("old",1,3,file)==3;valid&=fclose(file)==0;valid&=!remove("C:\\F79VIEW\\OUT1\\B.BIN");
 valid&=selectTwoFilesystemItems("A.BIN","B.BIN");fsReplaceTestChoice=IDNO;SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);
 valid&=lastResult==SZ_OK&&fsBatchCompleted==1&&fsBatchSkipped==1&&verifyExtractedFile("C:\\F79VIEW\\OUT1\\A.BIN",3,0x3f5dd4e5UL)&&verifyExtractedFile("C:\\F79VIEW\\OUT1\\B.BIN",12291,0xba8ed439UL);
 valid&=selectTwoFilesystemItems("A.BIN","B.BIN");fsReplaceTestChoice=IDYES;SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);
 valid&=lastResult==SZ_OK&&fsBatchCompleted==2&&!fsBatchSkipped&&verifyExtractedFile("C:\\F79VIEW\\OUT1\\A.BIN",8193,0x4cd01d7dUL)&&verifyExtractedFile("C:\\F79VIEW\\OUT1\\B.BIN",12291,0xba8ed439UL);
 fprintf(log,"%s batch Copy keeps distinct contents; No skips one collision and Yes replaces it\n",valid?"PASS":"FAIL");fflush(log);
 valid&=selectTwoFilesystemItems("A.BIN","B.BIN");strcpy(fsDestination,"OUT2");fsBatchCancelItem=2;SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);fsCancelTest=FALSE;fsBatchCancelItem=0;
 valid&=lastResult==SZ_ERROR_PROGRESS&&fsBatchCompleted==1&&verifyExtractedFile("C:\\F79VIEW\\OUT2\\A.BIN",8193,0x4cd01d7dUL)&&stat("C:\\F79VIEW\\OUT2\\B.BIN",&info)!=0&&verifyExtractedFile("C:\\F79VIEW\\B.BIN",12291,0xba8ed439UL);
 valid&=selectTwoFilesystemItems("A.BIN","B.BIN");SendMessage(mainWindow,WM_COMMAND,ID_MOVE,0);fsReplaceTestChoice=0;
 valid&=lastResult==SZ_OK&&fsBatchCompleted==2&&stat("C:\\F79VIEW\\A.BIN",&info)!=0&&stat("C:\\F79VIEW\\B.BIN",&info)!=0&&verifyExtractedFile("C:\\F79VIEW\\OUT2\\A.BIN",8193,0x4cd01d7dUL)&&verifyExtractedFile("C:\\F79VIEW\\OUT2\\B.BIN",12291,0xba8ed439UL);
 fprintf(log,"%s cancellation reports completed items; batch Move retains names after source rows disappear\n",valid?"PASS":"FAIL");fflush(log);
 loadDirectory("C:\\F79VIEW\\OUT2");valid&=selectTwoFilesystemItems("A.BIN","B.BIN");fsBatchDeleteTestChoice=IDNO;SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);
 valid&=lastResult==SZ_ERROR_PROGRESS&&!fsBatchCompleted&&!stat("C:\\F79VIEW\\OUT2\\A.BIN",&info)&&!stat("C:\\F79VIEW\\OUT2\\B.BIN",&info);
 fsBatchDeleteTestChoice=IDYES;SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);valid&=lastResult==SZ_OK&&fsBatchCompleted==2&&stat("C:\\F79VIEW\\OUT2\\A.BIN",&info)!=0&&stat("C:\\F79VIEW\\OUT2\\B.BIN",&info)!=0;
 fprintf(log,"%s batch Delete asks once, preserves declined selection and removes confirmed items\n",valid?"PASS":"FAIL");fflush(log);
 strcpy(parent,associationExecutable);slash=strrchr(parent,'\\');if(!slash)valid=0;else{
  strcpy(guard,slash+1);if(slash==parent+2)slash[1]=0;else *slash=0;loadDirectory(parent);
  if(directoryTestRow("GUARD79.TXT")>=0){fputs("FAIL private executable-guard fixture already exists\n",log);return 0;}
  {char path[260];valid&=folderJoin(parent,"GUARD79.TXT",path);file=fopen(path,"wb");if(!file)return 0;valid&=fputs("keep",file)>=0;valid&=fclose(file)==0;loadDirectory(parent);
   valid&=selectTwoFilesystemItems("GUARD79.TXT",guard);SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);
   valid&=lastResult==SZ_ERROR_UNSUPPORTED&&!stat(path,&info)&&!stat(associationExecutable,&info);valid&=!remove(path);}
 }
 loadDirectory("C:\\");selectListItem(directoryTestRow("F79VIEW"));SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);
 valid&=lastResult==SZ_OK&&stat("C:\\F79VIEW",&info)!=0&&windowAllocationCount==baseline;
 fprintf(log,"%s batch protects running executable before deleting any item; all fixtures and allocations released\n",valid?"PASS":"FAIL");fflush(log);return valid;
}
static int batchFilesystemSelfTest(FILE *log){
 int valid;
#ifdef __WATCOMC__
 unsigned segment=filesystemTestCs();
 if(!LockSegment(segment)){fputs("FAIL cannot retain batch test code\n",log);return 0;}
#endif
 valid=batchFilesystemSelfTestBody(log);
#ifdef __WATCOMC__
 UnlockSegment(segment);
#endif
 return valid;
}
#ifdef __WATCOMC__
#pragma code_seg("FS_TEST_TEXT")
#endif
static int filesystemSelfTestBody(FILE *log){
 int valid=1,row,i;FILE *file;struct stat info;UInt32 crc,size;char exe[260],parent[260],*slash;
 closeArchive();if(!stat("C:\\F64VIEW",&info)||mkdir("C:\\F64VIEW")){fputs("FAIL private filesystem fixture must be new\n",log);return 0;}
 file=fopen("C:\\F64VIEW\\SRC.BIN","wb");if(!file)return 0;for(i=0;i<10000;i++)if(fputc(i&255,file)==EOF)valid=0;if(fclose(file))valid=0;
 valid&=renameIdentity("C:\\F64VIEW\\SRC.BIN",&crc,&size);SetWindowText(pathEdit,"C:\\F64VIEW");openLocation();fsTesting=TRUE;
 row=directoryTestRow("SRC.BIN");selectListItem((int)(row));strcpy(fsDestination,"COPY.BIN");SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);
 valid&=lastResult==SZ_OK&&verifyExtractedFile("C:\\F64VIEW\\COPY.BIN",size,crc)&&!windowAllocationCount;fprintf(log,"%s Copy dialog publishes exact file and releases allocations\n",valid?"PASS":"FAIL");
 file=fopen("C:\\F64VIEW\\COPY.BIN","wb");if(!file)return 0;valid&=fwrite("old",1,3,file)==3;valid&=fclose(file)==0;
 row=directoryTestRow("SRC.BIN");selectListItem((int)(row));SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);valid&=lastResult==SZ_ERROR_PROGRESS&&verifyExtractedFile("C:\\F64VIEW\\COPY.BIN",3,0x3f5dd4e5UL);
 fsReplaceTestChoice=IDYES;SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);fsReplaceTestChoice=0;
 valid&=lastResult==SZ_OK&&verifyExtractedFile("C:\\F64VIEW\\COPY.BIN",size,crc)&&!windowAllocationCount;
 fprintf(log,"%s confirmed replacement publishes the new file; declining preserves the existing file\n",valid?"PASS":"FAIL");
 row=directoryTestRow("SRC.BIN");selectListItem((int)(row));strcpy(fsDestination,"CANCEL.BIN");fsCancelTest=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);valid&=lastResult==SZ_ERROR_PROGRESS&&stat("C:\\F64VIEW\\CANCEL.BIN",&info)!=0&&!windowAllocationCount;fsCancelTest=FALSE;
 fprintf(log,"%s existing destination preserved; cancellation removes unpublished copy\n",valid?"PASS":"FAIL");
 row=directoryTestRow("COPY.BIN");selectListItem((int)(row));strcpy(fsDestination,"MOVED.BIN");SendMessage(mainWindow,WM_COMMAND,ID_MOVE,0);valid&=lastResult==SZ_OK&&directoryTestRow("COPY.BIN")<0&&verifyExtractedFile("C:\\F64VIEW\\MOVED.BIN",size,crc);
 row=directoryTestRow("MOVED.BIN");selectListItem((int)(row));strcpy(fsDestination,"RENAMED.BIN");SendMessage(mainWindow,WM_COMMAND,ID_RENAME,0);valid&=lastResult==SZ_OK&&directoryTestRow("MOVED.BIN")<0&&verifyExtractedFile("C:\\F64VIEW\\RENAMED.BIN",size,crc);
 fprintf(log,"%s Move and Rename dialogs refresh actual directory entries\n",valid?"PASS":"FAIL");
 file=fopen("C:\\F64VIEW\\MOVED.BIN","wb");if(!file)return 0;valid&=fwrite("old",1,3,file)==3;valid&=fclose(file)==0;
 row=directoryTestRow("RENAMED.BIN");selectListItem((int)(row));strcpy(fsDestination,"MOVED.BIN");SendMessage(mainWindow,WM_COMMAND,ID_MOVE,0);
 valid&=lastResult==SZ_ERROR_PROGRESS&&verifyExtractedFile("C:\\F64VIEW\\RENAMED.BIN",size,crc)&&verifyExtractedFile("C:\\F64VIEW\\MOVED.BIN",3,0x3f5dd4e5UL);
 fsReplaceTestChoice=IDYES;fsCancelTest=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_MOVE,0);fsCancelTest=FALSE;
 valid&=lastResult==SZ_ERROR_PROGRESS&&verifyExtractedFile("C:\\F64VIEW\\RENAMED.BIN",size,crc)&&verifyExtractedFile("C:\\F64VIEW\\MOVED.BIN",3,0x3f5dd4e5UL)&&!windowAllocationCount;
 row=directoryTestRow("RENAMED.BIN");selectListItem((int)(row));SendMessage(mainWindow,WM_COMMAND,ID_MOVE,0);fsReplaceTestChoice=0;
 valid&=lastResult==SZ_OK&&directoryTestRow("RENAMED.BIN")<0&&verifyExtractedFile("C:\\F64VIEW\\MOVED.BIN",size,crc)&&!windowAllocationCount;
 row=directoryTestRow("MOVED.BIN");selectListItem((int)(row));strcpy(fsDestination,"RENAMED.BIN");SendMessage(mainWindow,WM_COMMAND,ID_RENAME,0);valid&=lastResult==SZ_OK;
 fprintf(log,"%s Move replacement: decline and cancellation preserve both files; confirmation publishes exact contents before removing source\n",valid?"PASS":"FAIL");
 strcpy(fsDestination,"FOLDER");SendMessage(mainWindow,WM_COMMAND,ID_MKDIR,0);valid&=lastResult==SZ_OK&&directoryTestRow("FOLDER")>0;
 valid&=!mkdir("C:\\F64VIEW\\FOLDER\\EMPTY")&&copyFixture("C:\\F64VIEW\\SRC.BIN","C:\\F64VIEW\\FOLDER\\HIDDEN.BIN");valid&=!_dos_setfileattr("C:\\F64VIEW\\FOLDER\\HIDDEN.BIN",_A_HIDDEN|_A_SYSTEM|_A_RDONLY);valid&=!_dos_setfileattr("C:\\F64VIEW\\FOLDER",_A_HIDDEN|_A_SYSTEM|_A_RDONLY);
 row=directoryTestRow("FOLDER");selectListItem((int)(row));strcpy(fsDestination,"TREECOPY");SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);
 valid&=lastResult==SZ_OK&&verifyExtractedFile("C:\\F64VIEW\\TREECOPY\\HIDDEN.BIN",size,crc)&&!stat("C:\\F64VIEW\\TREECOPY\\EMPTY",&info);
 fprintf(log,"%s New folder and recursive Copy retain empty descendants and hidden readonly files\n",valid?"PASS":"FAIL");
 row=directoryTestRow("SRC.BIN");selectListItem((int)(row));strcpy(fsDestination,"FOLDER");SendMessage(mainWindow,WM_COMMAND,ID_COPY,0);valid&=lastResult==SZ_OK&&verifyExtractedFile("C:\\F64VIEW\\FOLDER\\SRC.BIN",size,crc);
 row=directoryTestRow("TREECOPY");selectListItem((int)(row));SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);valid&=lastResult==SZ_OK&&stat("C:\\F64VIEW\\TREECOPY",&info)!=0;
 row=directoryTestRow("RENAMED.BIN");selectListItem((int)(row));SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);valid&=lastResult==SZ_OK&&stat("C:\\F64VIEW\\RENAMED.BIN",&info)!=0;
 fprintf(log,"%s Copy into an existing folder and confirmed file/tree Delete\n",valid?"PASS":"FAIL");
 valid&=batchFilesystemSelfTest(log);
 strcpy(parent,associationExecutable);slash=strrchr(parent,'\\');if(!slash)valid=0;else{
  strcpy(exe,slash+1);if(slash==parent+2)slash[1]=0;else *slash=0;loadDirectory(parent);row=directoryTestRow(exe);selectListItem((int)(row));SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);
  valid&=lastResult==SZ_ERROR_UNSUPPORTED&&!stat(associationExecutable,&info);
 }
 fprintf(log,"%s deleting the running executable is blocked before changes\n",valid?"PASS":"FAIL");
 loadDirectory("C:\\");row=directoryTestRow("F64VIEW");selectListItem((int)(row));SendMessage(mainWindow,WM_COMMAND,ID_DELETE,0);
 valid&=lastResult==SZ_OK&&stat("C:\\F64VIEW",&info)!=0&&!windowAllocationCount;fsTesting=FALSE;
 fprintf(log,"%s recursive fixture cleanup and zero outstanding allocations\n",valid?"PASS":"FAIL");fflush(log);return valid;
}
/* The test driver calls back through USER repeatedly. Keep its return frames
   resident only while active; permanent residency deprives the later real-mode
   LZMA tests of conventional memory. No interactive command loads this driver. */
static int filesystemSelfTest(FILE *log){
 int valid;
#ifdef __WATCOMC__
 unsigned segment=filesystemTestCs();
 if(!LockSegment(segment)){fputs("FAIL cannot retain filesystem test code\n",log);return 0;}
#endif
 valid=filesystemSelfTestBody(log);
#ifdef __WATCOMC__
 UnlockSegment(segment);
#endif
 return valid;
}

#ifdef __WATCOMC__
#pragma code_seg("GUI_TEST_TEXT")
#endif
#ifdef __WATCOMC__
#pragma code_seg("UI_TEST_TEXT")
#endif
static int sourceTextSelfTest(FILE *log){
 HGLOBAL handle=GlobalAlloc(GMEM_MOVEABLE,4097);char *text,*previous;int valid=1;
 if(!handle)return 0;text=(char *)GlobalLock(handle);if(!text){GlobalFree(handle);return 0;}
 memset(text,'A',4096);text[4095]=0;valid=setCreateSource(text);
 valid=valid&&strlen(createSource)==4095&&!memcmp(createSource,text,4096);previous=createSource;
 text[4095]='A';text[4096]=0;valid=valid&&!setCreateSource(text)&&createSource==previous&&strlen(createSource)==4095;
 valid=valid&&setCreateSource("short")&&!strcmp(createSource,"short");freeCreateSource();
 GlobalUnlock(handle);GlobalFree(handle);valid=valid&&!createSource&&!createSourceHandle;
 fprintf(log,"%s complete 4095-byte source text, over-limit preservation, replacement and cleanup\n",valid?"PASS":"FAIL");return valid;
}
/* Compare the same raw bytes through the actual bounded look stream.
   This isolates buffer overhead; it is not a compression benchmark. */
static int lookBufferTiming(FILE *log){
 Input source;CLookToRead2 stream;unsigned mode,repeat;DWORD started,elapsed[2];
 UInt32 reference=0,crc;UInt64 count;int valid=1;
 if(lookCapacity!=4096)return 1;
 source.iface.Read=readInput;source.iface.Seek=seekInput;
 for(mode=0;mode<2;mode++){
  started=GetTickCount();
  for(repeat=0;repeat<3;repeat++){
   source.file=fopen("C:\\COPY.7Z","rb");if(!source.file)return 0;
   LookToRead2_CreateVTable(&stream,False);stream.realStream=&source.iface;
   stream.buf=lookBuffer;stream.bufSize=mode?4096:512;LookToRead2_INIT(&stream);
   crc=CRC_INIT_VAL;count=0;
   for(;;){const void *data;size_t size=8192;SRes result=ILookInStream_Look(&stream.vt,&data,&size);
    if(result!=SZ_OK){valid=0;break;}if(!size)break;
    crc=CrcUpdate(crc,data,size);count+=size;ILookInStream_Skip(&stream.vt,size);
   }
   crc=CRC_GET_DIGEST(crc);if(!mode&&!repeat)reference=crc;
   valid&=crc==reference&&count==262389UL;valid&=fclose(source.file)==0;
  }
  elapsed[mode]=GetTickCount()-started;
 }
 fprintf(log,"%s three 262389-byte look-stream reads, identical CRC32 %08lX\n",valid?"PASS":"FAIL",reference);
 fprintf(log,"TIMING look-stream reads: 512-byte=%lu ms 4096-byte=%lu ms\n",elapsed[0],elapsed[1]);return valid;
}
static int toolbarSelfTest(FILE *log){
 static const unsigned ids[7]={115,ID_EXTRACT,ID_TEST,ID_COPY,ID_MOVE,ID_DELETE,ID_INFO};
 static const char *labels[7]={"Add","Extract","Test","Copy","Move","Delete","Info"};
 unsigned i,x,y,colored,transparent,ink,baseline;DWORD started,elapsed[2];int valid=1,fileHandle;char label[40],date[40];HDC dc,source;HBITMAP bitmap;HGDIOBJ old;DWORD key,pixel;FILE *file;struct stat info;HGLOBAL clip;char FAR *copied;CNtfsFileTime ft;
 closeArchive();toolbarPaintFailures=0;valid&=lookBufferTiming(log);
 valid&=lookCapacity==((GetWinFlags()&(WF_STANDARD|WF_ENHANCED))?4096:512);
 valid&=!!toolbarCache.ready==!!(GetWinFlags()&(WF_STANDARD|WF_ENHANCED));
 fprintf(log,"%s mode-specific read buffer (%u bytes) and %s toolbar renderer\n",valid?"PASS":"FAIL",lookCapacity,toolbarCache.ready?"cached bitmap":"indexed pixel");
 for(i=0;i<7;i++){
  HWND button=GetDlgItem(mainWindow,ids[i]);GetWindowText(button,label,sizeof(label));valid&=!strcmp(label,labels[i]);
  InvalidateRect(button,NULL,TRUE);UpdateWindow(button);dc=GetDC(button);bitmap=LoadBitmap(instance,MAKEINTRESOURCE(1001+i));source=CreateCompatibleDC(dc);colored=transparent=0;
  if(!dc||!bitmap||!source){valid=0;if(source)DeleteDC(source);if(bitmap)DeleteObject(bitmap);if(dc)ReleaseDC(button,dc);break;}
  old=SelectObject(source,bitmap);key=GetPixel(source,0,0);
  for(y=0;y<24;y++)for(x=0;x<24;x++){
   pixel=GetPixel(source,x,y);
   if(pixel==key){transparent++;valid&=GetPixel(dc,18+x,4+y)==GetSysColor(COLOR_BTNFACE);}
   else{colored++;valid&=GetPixel(dc,18+x,4+y)==pixel;}
  }
  ink=0;for(y=31;y<47;y++)for(x=4;x<56;x++)if(GetPixel(dc,x,y)!=GetSysColor(COLOR_BTNFACE))ink++;
  SelectObject(source,old);DeleteDC(source);DeleteObject(bitmap);ReleaseDC(button,dc);valid&=colored>0&&transparent>0&&ink>0;
  fprintf(log,"%s %s toolbar label (%u visible pixels), original 24x24 pixels and %u transparent background pixels\n",valid?"PASS":"FAIL",labels[i],ink,transparent);fflush(log);
 }
 if(toolbarCache.ready){
  for(x=0;x<2;x++){
   toolbarCache.ready=(BOOL)x;started=GetTickCount();
   for(y=0;y<4;y++)for(i=0;i<7;i++){
    HWND button=GetDlgItem(mainWindow,ids[i]);InvalidateRect(button,NULL,FALSE);UpdateWindow(button);
   }
   elapsed[x]=GetTickCount()-started;
  }
  fprintf(log,"TIMING 28 toolbar repaints: indexed=%lu ms cached=%lu ms\n",elapsed[0],elapsed[1]);
 }
 valid&=!toolbarPaintFailures;
 if(!stat("C:\\U65.BIN",&info)){fputs("FAIL properties fixture must be new\n",log);return 0;}
 file=fopen("C:\\U65.BIN","wb");if(!file)return 0;valid&=fwrite("abc",1,3,file)==3;valid&=fclose(file)==0;
 if(_dos_open("C:\\U65.BIN",O_RDWR,&fileHandle))return 0;
 valid&=!_dos_setftime(fileHandle,((2000-1980)<<9)|(2<<5)|29,(23<<11)|(59<<5)|29);valid&=!_dos_close(fileHandle);
 loadDirectory("C:\\");selectListItem((int)(directoryTestRow("U65.BIN")));SendMessage(mainWindow,WM_COMMAND,111,MAKELONG((unsigned)fileList,LBN_SELCHANGE));
 valid&=IsWindowEnabled(copyButton)&&IsWindowEnabled(moveButton)&&IsWindowEnabled(deleteButton)&&IsWindowEnabled(infoButton);
 lastResult=SZ_ERROR_PROGRESS;
 for(i=0;i<7;i++)for(x=1;x<=5;x++)SendMessage(mainWindow,WM_COMMAND,ids[i],MAKELONG((UINT)GetDlgItem(mainWindow,ids[i]),x));
 valid&=lastResult==SZ_ERROR_PROGRESS&&!stat("C:\\U65.BIN",&info)&&info.st_size==3;
 fprintf(log,"%s non-click button notifications do not invoke archive or file commands\n",valid?"PASS":"FAIL");
 propertiesTesting=TRUE;propertiesExpected="Size: 3 bytes";propertiesValid=FALSE;SendMessage(mainWindow,WM_COMMAND,ID_INFO,0);valid&=propertiesValid&&lastResult==SZ_OK&&!windowAllocationCount;
 if(OpenClipboard(mainWindow)){clip=(HGLOBAL)GetClipboardData(CF_TEXT);copied=clip?(char FAR *)GlobalLock(clip):NULL;valid&=copied&&strstr(copied,"Size: 3 bytes")!=NULL&&strstr(copied,"Modified (local): 2000-02-29 23:59:58")!=NULL;if(copied)GlobalUnlock(clip);CloseClipboard();}else valid=0;
 fprintf(log,"%s selected filesystem properties and Copy clipboard button\n",valid?"PASS":"FAIL");fflush(log);
 SetWindowText(pathEdit,"C:\\WIN16.ZIP");SendMessage(mainWindow,WM_COMMAND,ID_OPEN,0);valid&=opened&&!IsWindowEnabled(copyButton)&&!IsWindowEnabled(moveButton)&&IsWindowEnabled(infoButton);
 selectListItem((int)(0));propertiesExpected="CRC32: 2783538A";propertiesValid=FALSE;baseline=windowAllocationCount;SendMessage(mainWindow,WM_COMMAND,ID_INFO,0);
 valid&=lastResult==SZ_OK&&propertiesValid&&windowAllocationCount==baseline;
 selectListItem((int)((WPARAM)-1));propertiesExpected="Total unpacked size: 306000 bytes";propertiesValid=FALSE;SendMessage(mainWindow,WM_COMMAND,ID_INFO,0);valid&=lastResult==SZ_OK&&propertiesValid&&windowAllocationCount==baseline;
 propertiesTesting=FALSE;performTest();valid&=lastResult==SZ_OK;
 closeArchive();
 fprintf(log,"%s selected archive-entry properties, archive summary and unchanged integrity\n",valid?"PASS":"FAIL");fflush(log);
 ft.Low=ft.High=0;propertiesNtfsTime(&ft,date);valid&=!strcmp(date,"1601-01-01 00:00:00");
 ft.Low=0x15051300UL;ft.High=0x01bf8311UL;propertiesNtfsTime(&ft,date);valid&=!strcmp(date,"2000-02-29 23:59:58");
 ft.Low=0xb4485800UL;ft.High=0x022fa029UL;propertiesNtfsTime(&ft,date);valid&=!strcmp(date,"2100-03-01 12:34:56");
 valid&=!remove("C:\\U65.BIN")&&!windowAllocationCount;
 fprintf(log,"%s archive UTC dates across leap centuries and no retained properties allocation\n",valid?"PASS":"FAIL");fflush(log);return valid;
}

#ifdef __WATCOMC__
#pragma code_seg("GUI_TEST_TEXT")
#endif
#include "selection_gui_tests.h"
#include "deflate64_gui_tests.h"
#ifdef __WATCOMC__
#pragma code_seg("GUI_TEST_TEXT")
#endif
static void runSelfTest(void){
 FILE *log=fopen("C:\\GUI16.TXT","w");int valid=1;long items;HICON icon;unsigned cycle;
 if(!log){PostMessage(mainWindow,WM_CLOSE,0,0);return;}
 fprintf(log,"Native Windows archive browser GetWinFlags=%lu\n",GetWinFlags());
 aboutTesting=TRUE;valid=showAbout(mainWindow)==1;aboutTesting=FALSE;fprintf(log,"%s original-size transparent About logo and x86-16 label\n",valid?"PASS":"FAIL");
 icon=LoadIcon(instance,MAKEINTRESOURCE(1));valid&=icon!=0;fprintf(log,"%s application icon resource\n",valid?"PASS":"FAIL");
 if(deflate64OnlyTest){valid&=deflate64SelfTest(log);goto finishTests;}
 if(selectionOnlyTest){valid&=selectionSelfTest(log);goto finishTests;}
 if(uiOnlyTest){valid&=sourceTextSelfTest(log);valid&=toolbarSelfTest(log);valid&=browseSelfTest(log);valid&=filesystemSelfTest(log);if(uiUpdateTest){valid&=updateSelfTest(log);valid&=treeSelfTest(log);valid&=hashSelfTest(log);}goto finishTests;}
 if(filesOnlyTest){valid&=browseSelfTest(log);valid&=filesystemSelfTest(log);if(filesUpdateTest){valid&=updateSelfTest(log);valid&=treeSelfTest(log);valid&=hashSelfTest(log);}goto finishTests;}
 if(browseOnlyTest){valid&=browseSelfTest(log);goto finishTests;}
 if(dictionaryOnlyTest){valid&=dictionarySelfTest(log);goto finishTests;}
 if(filterOnlyTest){valid&=filterSelfTest(log);if(bcjUpdateTest){valid&=updateSelfTest(log);valid&=treeSelfTest(log);valid&=hashSelfTest(log);}goto finishTests;}
 if(updateOnlyTest){valid&=browseSelfTest(log);valid&=updateSelfTest(log);valid&=treeSelfTest(log);valid&=hashSelfTest(log);goto finishTests;}
 if(hashOnlyTest){if(hashAfterAssociation)valid&=associationSelfTest(log);valid&=hashSelfTest(log);valid&=hashRecoverySelfTest(log);goto finishTests;}
 if(associationOnlyTest){valid&=associationSelfTest(log);goto finishTests;}
 if(treeOnlyTest){valid&=treeSelfTest(log);goto finishTests;}
 if(flatOnlyTest){valid&=flatExtractionSelfTest(log);goto finishTests;}
 valid&=browseSelfTest(log);
 SetWindowText(pathEdit,"C:\\MULTI.7Z");SendMessage(mainWindow,WM_COMMAND,ID_OPEN,0);items=SendMessage(fileList,LB_GETCOUNT,0,0);
 valid&=opened&&lastResult==SZ_OK&&items==6;fprintf(log,"%s list actual solid archive; items=%ld result=%d\n",valid?"PASS":"FAIL",items,lastResult);
 SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);fprintf(log,"%s test actual solid archive; result=%d\n",lastResult==SZ_OK?"PASS":"FAIL",lastResult);valid&=lastResult==SZ_OK;
 PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);fprintf(log,"%s cooperative cancellation; result=%d\n",lastResult==SZ_ERROR_PROGRESS?"PASS":"FAIL",lastResult);valid&=lastResult==SZ_ERROR_PROGRESS;
 SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);fprintf(log,"%s repeat after cancellation; result=%d\n",lastResult==SZ_OK?"PASS":"FAIL",lastResult);valid&=lastResult==SZ_OK;
 SetWindowText(pathEdit,"C:\\BADCRC.7Z");SendMessage(mainWindow,WM_COMMAND,ID_OPEN,0);SendMessage(mainWindow,WM_COMMAND,ID_TEST,0);fprintf(log,"%s corrupted file CRC; result=%d\n",lastResult==SZ_ERROR_CRC?"PASS":"FAIL",lastResult);valid&=lastResult==SZ_ERROR_CRC;
 SetWindowText(pathEdit,"C:\\TRUNC.7Z");SendMessage(mainWindow,WM_COMMAND,ID_OPEN,0);fprintf(log,"%s truncated archive is rejected; result=%d\n",lastResult!=SZ_OK&&!opened?"PASS":"FAIL",lastResult);valid&=lastResult!=SZ_OK&&!opened;
 valid&=extractionSelfTest(log);
 valid&=creationSelfTest(log);
 valid&=metadataSelfTest(log);
 valid&=deflateSelfTest(log);
 valid&=zipSelfTest(log);
 valid&=renameSelfTest(log);
 valid&=updateSelfTest(log);
 valid&=zipCreationSelfTest(log);
 valid&=zipUpdateSelfTest(log);
 valid&=flatExtractionSelfTest(log);
 valid&=treeSelfTest(log);
 valid&=associationSelfTest(log);
 valid&=hashSelfTest(log);
 valid&=hashRecoverySelfTest(log);
 valid&=dictionarySelfTest(log);
 valid&=filterSelfTest(log);
 finishTests:
 for(cycle=0;cycle<8;cycle++){
  SetWindowText(pathEdit,"C:\\MULTI.7Z");loadArchive();if(!opened||lastResult!=SZ_OK)valid=0;closeArchive();if(windowAllocationCount)valid=0;
 }
 fprintf(log,"%s repeated open/close; outstanding archive blocks=%u\n",!windowAllocationCount?"PASS":"FAIL",windowAllocationCount);
 fprintf(log,"%s ALL GUI TESTS\n",valid?"PASS":"FAIL");fclose(log);SetWindowText(pathEdit,"C:\\MULTI.7Z");loadArchive();MessageBox(mainWindow,valid?"PASS: native archive browser tests.":"FAIL: native archive browser tests.","Windows 3.0/3.1x tests",MB_OK);
}
#ifdef __WATCOMC__
#pragma code_seg()
#endif
LRESULT FAR PASCAL __export WindowProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 switch(message){
 case WM_CREATE:{
  mainWindow=window;
  pathEdit=CreateWindow("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,8,64,532,24,window,(HMENU)110,instance,NULL);
  SendMessage(pathEdit,EM_LIMITTEXT,259,0);
  openButton=CreateWindow("BUTTON","Open",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,548,64,64,24,window,(HMENU)ID_OPEN,instance,NULL);
  createButton=CreateWindow("BUTTON","Add",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,8,6,60,50,window,(HMENU)115,instance,NULL);
  extractButton=CreateWindow("BUTTON","Extract",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,70,6,60,50,window,(HMENU)ID_EXTRACT,instance,NULL);
  testButton=CreateWindow("BUTTON","Test",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,132,6,60,50,window,(HMENU)ID_TEST,instance,NULL);
  copyButton=CreateWindow("BUTTON","Copy",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,194,6,60,50,window,(HMENU)ID_COPY,instance,NULL);
  moveButton=CreateWindow("BUTTON","Move",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,256,6,60,50,window,(HMENU)ID_MOVE,instance,NULL);
  deleteButton=CreateWindow("BUTTON","Delete",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,318,6,60,50,window,(HMENU)ID_DELETE,instance,NULL);
  infoButton=CreateWindow("BUTTON","Info",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,380,6,60,50,window,(HMENU)ID_INFO,instance,NULL);
  cancelButton=CreateWindow("BUTTON","Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,548,18,64,24,window,(HMENU)ID_CANCEL,instance,NULL);
  listHeader=CreateWindow("Z7Hdr",NULL,WS_CHILD|WS_VISIBLE,8,Z7_HDR_TOP,596,Z7_HDR_H,window,(HMENU)113,instance,NULL);
  if(!configureFileList(FALSE))return -1;
  statusText=CreateWindow("STATIC","Open a folder or a 7z/ZIP archive.",WS_CHILD|WS_VISIBLE,8,356,616,32,window,(HMENU)112,instance,NULL);
  EnableWindow(testButton,FALSE);EnableWindow(extractButton,FALSE);EnableWindow(cancelButton,FALSE);updateToolbarState();return 0;}
 case WM_SIZE:{int width=LOWORD(lParam),height=HIWORD(lParam);if(width<540)width=540;if(height<180)height=180;
  MoveWindow(pathEdit,8,64,width-88,24,TRUE);MoveWindow(openButton,width-72,64,64,24,TRUE);MoveWindow(cancelButton,width-72,18,64,24,TRUE);MoveWindow(listHeader,8,Z7_HDR_TOP,width-16,Z7_HDR_H,TRUE);MoveWindow(fileList,8,Z7_LIST_TOP,width-16,height-(Z7_LIST_TOP+44),TRUE);MoveWindow(statusText,8,height-38,width-16,32,TRUE);return 0;}
 case WM_MEASUREITEM:{MEASUREITEMSTRUCT FAR *mi=(MEASUREITEMSTRUCT FAR *)lParam;if(mi->CtlType==ODT_LISTBOX&&mi->CtlID==111)mi->itemHeight=16;return TRUE;}
 case WM_DRAWITEM:{UINT resource=0;const DRAWITEMSTRUCT FAR *item=(const DRAWITEMSTRUCT FAR *)lParam;
  if(item->CtlType==ODT_LISTBOX&&item->CtlID==111){drawListItem(item);return TRUE;}
  switch(wParam){case 115:resource=1001;break;case ID_EXTRACT:resource=1002;break;case ID_TEST:resource=1003;break;case ID_COPY:resource=1004;break;case ID_MOVE:resource=1005;break;case ID_DELETE:resource=1006;break;case ID_INFO:resource=1007;break;}
  if(resource){if(!Z7Win16_DrawToolbar(instance,item,resource))toolbarPaintFailures++;return TRUE;}break;}
 case WM_COMMAND:
  /* A control notification is not necessarily a click. In particular, legacy
     button paint/highlight/disable notifications must never invoke an action
     or recursively refresh button state. Menus have no control handle. */
  if(LOWORD(lParam)&&wParam!=111&&HIWORD(lParam)!=BN_CLICKED)return 0;
  switch(wParam){case 111:if(HIWORD(lParam)==LBN_DBLCLK)openFolderSelection();break;case ID_PARENT:parentLocation();break;case ID_REFRESH:openLocation();break;case ID_OPEN:openLocation();break;case ID_TEST:performTest();break;case ID_EXTRACT:performExtract();break;case ID_CREATE:performCreate(0);break;case 115:performCreate(opened);break;case ID_UPDATE:performCreate(1);break;case ID_COPY:performFilesystem(Z7_FS_COPY);break;case ID_MOVE:performFilesystem(Z7_FS_MOVE);break;case ID_MKDIR:performFilesystem(Z7_FS_MKDIR);break;case ID_DELETE:if(folderView)performFilesystem(Z7_FS_DELETE);else performDelete();break;case ID_RENAME:if(folderView)performFilesystem(Z7_FS_RENAME);else performRename();break;case ID_CANCEL:cancelled=1;break;case ID_CLOSE:if(!busy)closeArchive();break;case ID_EXIT:SendMessage(window,WM_CLOSE,0,0);break;
  case ID_INFO:showProperties();break;
  case ID_HASH:performHash();break;
 case ID_ASSOC:if(!busy)showAssociations(window);break;
 case ID_ABOUT:showAbout(window);break;}updateToolbarState();return 0;
 case ID_SELFTEST:runSelfTest();return 0;
 case WM_CLOSE:if(busy){cancelled=1;closePending=1;return 0;}closeArchive();DestroyWindow(window);return 0;
 case WM_DESTROY:PostQuitMessage(0);return 0;
 }
 return DefWindowProc(window,message,wParam,lParam);
}
int PASCAL WinMain(HINSTANCE current,HINSTANCE previous,LPSTR command,int show){
 WNDCLASS wc;HWND window;HMENU menu,file,tools,help;MSG message;unsigned moduleLength;
 instance=current;moduleLength=GetModuleFileName(current,associationExecutable,sizeof(associationExecutable));if(!moduleLength||moduleLength>=sizeof(associationExecutable)-1||!validDosDestination(associationExecutable))return 2;hashAfterAssociation=command&&strstr(command,"/ASSOHASH")!=NULL;hashOnlyTest=hashAfterAssociation||(command&&strstr(command,"/HASHTEST")!=NULL);associationOnlyTest=command&&strstr(command,"/ASSOCTEST")!=NULL;flatOnlyTest=command&&strstr(command,"/FLATTEST")!=NULL;treeOnlyTest=command&&strstr(command,"/TREETEST")!=NULL;dictionaryOnlyTest=command&&strstr(command,"/DICTTEST")!=NULL;bcjOnlyTest=command&&strstr(command,"/BCJ2")!=NULL;bcjUpdateTest=command&&strstr(command,"/BCJ2UPDATE")!=NULL;filterOnlyTest=bcjOnlyTest||(command&&strstr(command,"/FILTERTEST")!=NULL);updateOnlyTest=command&&strstr(command,"/UPDATETEST")!=NULL;browseOnlyTest=command&&strstr(command,"/BROWSETEST")!=NULL;filesOnlyTest=command&&strstr(command,"/FILESTEST")!=NULL;filesUpdateTest=command&&strstr(command,"/FILESTESTUPDATE")!=NULL;uiOnlyTest=command&&strstr(command,"/UITEST")!=NULL;uiUpdateTest=command&&strstr(command,"/UITESTUPDATE")!=NULL;selectionOnlyTest=command&&strstr(command,"/SELECTTEST")!=NULL;deflate64OnlyTest=command&&strstr(command,"/D64TEST")!=NULL;selfTest=deflate64OnlyTest||selectionOnlyTest||uiOnlyTest||filesOnlyTest||browseOnlyTest||updateOnlyTest||filterOnlyTest||dictionaryOnlyTest||hashOnlyTest||associationOnlyTest||treeOnlyTest||flatOnlyTest||(command&&strstr(command,"/SELFTEST")!=NULL);CrcGenerateTable();
 if(!initLookBuffer())return 2;
 if(!Z7Win16_InitToolbar(current)){freeLookBuffer();return 2;}
 if(!previous){memset(&wc,0,sizeof(wc));wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=WindowProc;wc.hInstance=current;wc.hCursor=LoadCursor(0,IDC_ARROW);wc.hIcon=LoadIcon(current,MAKEINTRESOURCE(1));wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName="Z7Win16Browser";if(!RegisterClass(&wc)){Z7Win16_FreeToolbar();freeLookBuffer();return 2;}
  wc.lpfnWndProc=Z7HeaderProc;wc.hIcon=0;wc.lpszClassName="Z7Hdr";RegisterClass(&wc);}
 menu=CreateMenu();file=CreatePopupMenu();tools=CreatePopupMenu();help=CreatePopupMenu();AppendMenu(file,MF_STRING,ID_CREATE,"&New archive...");AppendMenu(file,MF_STRING,ID_OPEN,"&Open");AppendMenu(file,MF_STRING,ID_CLOSE,"Close &archive");AppendMenu(file,MF_STRING,ID_COPY,"&Copy to...");AppendMenu(file,MF_STRING,ID_MOVE,"&Move to...");AppendMenu(file,MF_STRING,ID_MKDIR,"New &folder...");AppendMenu(file,MF_STRING,ID_PARENT,"&Up one level\tBackspace");AppendMenu(file,MF_STRING,ID_REFRESH,"&Refresh\tF5");AppendMenu(file,MF_SEPARATOR,0,NULL);AppendMenu(file,MF_STRING,ID_EXIT,"E&xit");AppendMenu(tools,MF_STRING,ID_EXTRACT,"&Extract archive...");AppendMenu(tools,MF_STRING,ID_TEST,"&Test archive");AppendMenu(tools,MF_STRING,ID_UPDATE,"&Add or replace files...");AppendMenu(tools,MF_STRING,ID_DELETE,"&Delete entries...");AppendMenu(tools,MF_STRING,ID_RENAME,"&Rename entry...\tF2");AppendMenu(tools,MF_STRING,ID_INFO,"File &properties...");AppendMenu(tools,MF_SEPARATOR,0,NULL);AppendMenu(tools,MF_STRING,ID_HASH,"File &checksums...");AppendMenu(tools,MF_STRING,ID_ASSOC,"File &associations...");AppendMenu(help,MF_STRING,ID_ABOUT,"&About 7-Zip");AppendMenu(menu,MF_POPUP,(UINT)file,"&File");AppendMenu(menu,MF_POPUP,(UINT)tools,"&Tools");AppendMenu(menu,MF_POPUP,(UINT)help,"&Help");
 window=CreateWindow("Z7Win16Browser","7-Zip 26.05 - Windows 3.0/3.1x (16-bit)",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,620,460,0,menu,current,NULL);if(!window){Z7Win16_FreeToolbar();freeLookBuffer();return 2;}ShowWindow(window,show);UpdateWindow(window);SetFocus(pathEdit);if(selfTest)PostMessage(window,ID_SELFTEST,0,0);
 else {char initial[260];int parsed=openArgument(command,initial);if(parsed==1){SetWindowText(pathEdit,initial);PostMessage(window,WM_COMMAND,ID_OPEN,0);}else if(parsed<0)MessageBox(window,"Enter one folder or archive path using DOS 8.3 components. Quote the path if needed.","Open",MB_OK|MB_ICONEXCLAMATION);else if(getcwd(initial,sizeof(initial))){SetWindowText(pathEdit,initial);openLocation();}}
 while(GetMessage(&message,0,0,0)){if(message.hwnd==fileList&&(message.message==WM_LBUTTONDOWN||message.message==WM_RBUTTONDOWN))selectionAnchor=-1;if(message.message==WM_KEYDOWN&&message.wParam==VK_F5){SendMessage(window,WM_COMMAND,ID_REFRESH,0);continue;}if(message.message==WM_KEYDOWN&&message.hwnd==fileList){if(filesystemSelectionKey(message.wParam))continue;if(message.wParam==VK_RETURN){openFolderSelection();continue;}if(message.wParam==VK_BACK){parentLocation();continue;}}if(message.message==WM_KEYDOWN&&message.wParam==VK_F2){SendMessage(window,WM_COMMAND,ID_RENAME,0);continue;}if(message.message==WM_KEYDOWN&&message.wParam==VK_RETURN&&message.hwnd==pathEdit){SendMessage(window,WM_COMMAND,ID_OPEN,0);continue;}/* Keep menu Alt mnemonics in the normal window loop. IsDialogMessage
    consumes WM_SYSCHAR even though this main window is not a dialog. */
  if(!(message.message==WM_KEYDOWN&&(message.wParam==VK_TAB||message.wParam==VK_RETURN))||!IsDialogMessage(window,&message)){TranslateMessage(&message);DispatchMessage(&message);}}
 freeCreateSource();Z7Win16_FreeToolbar();freeLookBuffer();return message.wParam;
}
