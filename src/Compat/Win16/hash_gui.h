/* Native checksum dialogs. Operate on the explicitly named file on disk. */
#ifndef Z7_WIN16_HASH_GUI_H
#define Z7_WIN16_HASH_GUI_H
#ifdef Z7_HASH_UI_TRACE
#include <conio.h>
#define HASH_UI_TRACE(c) outp(0xE9,(c))
#else
#define HASH_UI_TRACE(c) ((void)0)
#endif
BOOL FAR PASCAL __export HashInputProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 unsigned i;(void)lParam;
 switch(message){
 case WM_INITDIALOG:
  SetDlgItemText(window,701,hashSource);SendDlgItemMessage(window,701,EM_LIMITTEXT,259,0);
  SendDlgItemMessage(window,702,CB_ADDSTRING,0,(LPARAM)(LPSTR)"All methods");
  for(i=0;i<Z7_WIN16_HASH_COUNT;i++)SendDlgItemMessage(window,702,CB_ADDSTRING,0,(LPARAM)(LPSTR)Z7Win16_HashMethods[i].name);
  SendDlgItemMessage(window,702,CB_SETCURSEL,hashChoice,0);if(hashTesting)PostMessage(window,WM_COMMAND,IDOK,0);return TRUE;
 case WM_COMMAND:
  if(wParam==IDOK){int choice;GetDlgItemText(window,701,hashSource,sizeof(hashSource));choice=(int)SendDlgItemMessage(window,702,CB_GETCURSEL,0,0);
   if(!validDosDestination(hashSource)||choice<0||choice>Z7_WIN16_HASH_COUNT){if(!hashTesting)MessageBox(window,"Enter a file path using DOS 8.3 components and select a checksum method.","File checksums",MB_OK);else EndDialog(window,0);return TRUE;}
   hashChoice=(unsigned)choice;EndDialog(window,1);return TRUE;
  }
  if(wParam==IDCANCEL){EndDialog(window,0);return TRUE;}break;
 case WM_CLOSE:EndDialog(window,0);return TRUE;
 }
 return FALSE;
}
BOOL FAR PASCAL __export HashResultProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 (void)lParam;
 switch(message){
 case WM_INITDIALOG:SetDlgItemText(window,711,hashResults);if(hashTesting){PostMessage(window,WM_COMMAND,712,0);PostMessage(window,WM_COMMAND,IDOK,0);}return TRUE;
 case WM_COMMAND:
  if(wParam==712){HWND edit=GetDlgItem(window,711);SendMessage(edit,EM_SETSEL,0,MAKELONG(0,32767));SendMessage(edit,WM_COPY,0,0);return TRUE;}
  if(wParam==IDOK||wParam==IDCANCEL){EndDialog(window,1);return TRUE;}break;
 case WM_CLOSE:EndDialog(window,1);return TRUE;
 }
 return FALSE;
}
static void performHash(void){
 FARPROC proc;int choice;unsigned mask,m,j,k=0;Byte digests[64*Z7_WIN16_HASH_COUNT];UInt64 size;char *cursor;
 if(busy)return;if(!hashTesting)GetWindowText(pathEdit,hashSource,sizeof(hashSource));
 HASH_UI_TRACE('H');
 proc=MakeProcInstance((FARPROC)HashInputProc,instance);if(!proc){HASH_UI_TRACE('p');lastResult=SZ_ERROR_MEM;hashResults[0]=0;return;}choice=DialogBox(instance,MAKEINTRESOURCE(700),mainWindow,(DLGPROC)proc);FreeProcInstance(proc);if(choice!=1){HASH_UI_TRACE('d');if(choice<0){lastResult=SZ_ERROR_MEM;hashResults[0]=0;SetWindowText(statusText,"Not enough memory to open the checksum dialog.");}return;}HASH_UI_TRACE('i');
 hashResults[0]=0;mask=hashChoice?1u<<(hashChoice-1):(1u<<Z7_WIN16_HASH_COUNT)-1;
 busy=1;cancelled=closePending=0;operationText="Calculating checksums";
 EnableWindow(openButton,FALSE);EnableWindow(createButton,FALSE);EnableWindow(pathEdit,FALSE);EnableWindow(testButton,FALSE);EnableWindow(extractButton,FALSE);EnableWindow(cancelButton,TRUE);
 HASH_UI_TRACE('b');lastResult=Z7Win16_HashFileMany(hashSource,mask,digests,&size,&allocator,progress);HASH_UI_TRACE(lastResult==SZ_OK?'o':lastResult==SZ_ERROR_MEM?'m':'e');
 /* Windows 3.0 real mode can fail a small allocation while discardable
    checksum code is locked. The backend has now released its contexts and
    code lock. No digest is published on failure; compact here, outside those
    active checksum frames, and make one bounded attempt. */
 if(lastResult==SZ_ERROR_MEM&&!(GetWinFlags()&WF_PMODE)){
  if(!progress(0))lastResult=SZ_ERROR_PROGRESS;
  else{GlobalCompact(0xFFFFFFFFUL);lastResult=Z7Win16_HashFileMany(hashSource,mask,digests,&size,&allocator,progress);}
 }
 busy=0;EnableWindow(openButton,TRUE);EnableWindow(createButton,TRUE);EnableWindow(pathEdit,TRUE);EnableWindow(testButton,opened);EnableWindow(extractButton,opened);EnableWindow(cancelButton,FALSE);
 if(lastResult==SZ_OK){
  unsigned needed=(unsigned)strlen(hashSource)+5;
  for(m=0;m<Z7_WIN16_HASH_COUNT;m++)if(mask&(1u<<m))needed+=(unsigned)strlen(Z7Win16_HashMethods[m].name)+6+2*Z7Win16_HashMethods[m].digestSize;
  if(needed>sizeof(hashResults)){HASH_UI_TRACE('v');lastResult=SZ_ERROR_MEM;hashResults[0]=0;}
 }
 if(lastResult==SZ_OK){
  strcpy(hashResults,hashSource);strcat(hashResults,"\r\n\r\n");cursor=hashResults+strlen(hashResults);
  for(m=0;m<Z7_WIN16_HASH_COUNT;m++)if(mask&(1u<<m)){
   strcpy(cursor,Z7Win16_HashMethods[m].name);cursor+=strlen(cursor);*cursor++='\r';*cursor++='\n';
   for(j=0;j<Z7Win16_HashMethods[m].digestSize;j++){unsigned value=digests[k*64+j];*cursor++="0123456789ABCDEF"[value>>4];*cursor++="0123456789ABCDEF"[value&15];}k++;
   *cursor++='\r';*cursor++='\n';*cursor++='\r';*cursor++='\n';*cursor=0;
  }
  SetWindowText(statusText,"File checksums calculated.");proc=MakeProcInstance((FARPROC)HashResultProc,instance);if(proc){DialogBox(instance,MAKEINTRESOURCE(710),mainWindow,(DLGPROC)proc);FreeProcInstance(proc);}
 }else{
  const char *text=lastResult==SZ_ERROR_PROGRESS?"Checksum calculation cancelled.":lastResult==SZ_ERROR_MEM?"Not enough memory to calculate these checksums.":"Could not read the file, or the file changed while it was being read.";
  SetWindowText(statusText,text);if(!hashTesting&&lastResult!=SZ_ERROR_PROGRESS)MessageBox(mainWindow,text,"File checksums",MB_OK|MB_ICONEXCLAMATION);
 }
 if(closePending)PostMessage(mainWindow,WM_CLOSE,0,0);
}
#endif
