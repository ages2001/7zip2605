/* Native filesystem commands for the Windows 3.x browser.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#include "filesystem_ops.h"
#define ID_COPY 120
#define ID_MOVE 121
#define ID_MKDIR 122
static BOOL fsTesting,filesOnlyTest,filesUpdateTest,fsCancelTest;
static int fsReplaceTestChoice;
static int fsBatchDeleteTestChoice=IDYES;
static unsigned fsBatchCancelItem,fsBatchCompleted,fsBatchSkipped;
static char fsDestination[260];
static const char *fsTitle,*fsPrompt;
BOOL FAR PASCAL __export FilesystemProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 (void)lParam;
 switch(message){
 case WM_INITDIALOG:SetWindowText(window,fsTitle);SetDlgItemText(window,801,fsPrompt);SetDlgItemText(window,802,fsDestination);SendDlgItemMessage(window,802,EM_LIMITTEXT,259,0);if(fsTesting)PostMessage(window,WM_COMMAND,IDOK,0);return TRUE;
 case WM_COMMAND:
  if(wParam==IDOK){GetDlgItemText(window,802,fsDestination,sizeof(fsDestination));if(!fsDestination[0]){if(!fsTesting)MessageBox(window,"Enter a destination name or path.",fsTitle,MB_OK);else EndDialog(window,0);return TRUE;}EndDialog(window,1);return TRUE;}
  if(wParam==IDCANCEL){EndDialog(window,0);return TRUE;}break;
 case WM_CLOSE:EndDialog(window,0);return TRUE;
 }
 return FALSE;
}
#ifdef __WATCOMC__
#pragma code_seg("FS_GUI_TEXT")
#endif
static int folderJoin(const char *base,const char *name,char *out){
 size_t n=strlen(base);if(n+strlen(name)+2>260)return 0;strcpy(out,base);if(n&&out[n-1]!='\\'&&out[n-1]!='/')out[n++]='\\';strcpy(out+n,name);return 1;
}
static int folderSelectedPath(char *out,char *name){
 long selected,length;char row[80],*tab;
 selected=singleListSelection();if(selected<0)return 0;
 length=SendMessage(fileList,LB_GETTEXTLEN,(WPARAM)selected,0);if(length<0||length>=sizeof(row))return 0;
 SendMessage(fileList,LB_GETTEXT,(WPARAM)selected,(LPARAM)(LPSTR)row);tab=strchr(row,'\t');if(tab)*tab=0;
 if(!validDosComponent(row,(unsigned)strlen(row)))return 0;strcpy(name,row);return folderJoin(folderPath,row,out);
}
static int fsGuiProgress(UInt64 bytes){if(fsCancelTest&&bytes>=4096){fsCancelTest=FALSE;PostMessage(mainWindow,WM_COMMAND,ID_CANCEL,0);}return progress(bytes);}
static int protectsRunningManager(const char *source){return sameName(source,associationExecutable)||namePrefix(source,associationExecutable);}
/* Each selected item keeps the backend's staging/rollback guarantees. A batch
   can be partially complete; never describe earlier published items as rolled
   back when a later item is cancelled or fails. */
static void performFilesystemBatch(unsigned operation,unsigned count){
 char source[260],target[260],saved[260],destination[260],row[80],message[420],warning[320],label[48],*names,*tab;
 Byte *snapshot=NULL;int *indices,choice;unsigned i,selectedOperation;long length;
 FARPROC proc;Z7FsReport report;SRes result=SZ_ERROR_PARAM;struct stat info;int began=0;
 fsBatchCompleted=fsBatchSkipped=0;warning[0]=message[0]=0;strcpy(saved,folderPath);
 if(count>256){strcpy(message,"Select at most 256 items for one operation.");goto done;}
 snapshot=(Byte *)ISzAlloc_Alloc(&allocator,count*(sizeof(int)+13));
 if(!snapshot){result=SZ_ERROR_MEM;strcpy(message,"Not enough memory to retain the selection. No files were changed.");goto done;}
 indices=(int *)snapshot;names=(char *)(indices+count);
 if(SendMessage(fileList,LB_GETSELITEMS,count,(LPARAM)(LPINT)indices)!=(long)count){strcpy(message,"The selection changed. Select the files again.");goto done;}
 /* Save names, not row indices: completed moves/deletes can reorder the list. */
 for(i=0;i<count;i++){
  length=SendMessage(fileList,LB_GETTEXTLEN,indices[i],0);
  if(length<0||length>=sizeof(row)){strcpy(message,"Cannot read the selected item.");goto done;}
  SendMessage(fileList,LB_GETTEXT,indices[i],(LPARAM)(LPSTR)row);tab=strchr(row,'\t');if(tab)*tab=0;
  if(!validDosComponent(row,(unsigned)strlen(row))){strcpy(message,"Select files or folders. The parent entry cannot be changed.");goto done;}
  strcpy(names+13*i,row);
  if(!folderJoin(saved,row,source)){strcpy(message,"A selected path is too long.");goto done;}
  if(operation!=Z7_FS_COPY&&protectsRunningManager(source)){
   result=SZ_ERROR_UNSUPPORTED;strcpy(message,"Close 7-Zip before moving or deleting its executable or installation folder. No files were changed.");goto done;
  }
 }
 if(operation==Z7_FS_DELETE){
  sprintf(message,"Permanently delete %u selected items? All contents of selected folders will also be deleted. This operation cannot be undone.",count);
  choice=fsTesting?fsBatchDeleteTestChoice:MessageBox(mainWindow,message,fsTitle,MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
  if(choice!=IDYES){result=SZ_ERROR_PROGRESS;strcpy(message,"Operation cancelled. No files were changed.");goto done;}
 }else{
  fsPrompt="Existing destination folder for all selected items:";
  if(!fsTesting)strcpy(fsDestination,saved);
  proc=MakeProcInstance((FARPROC)FilesystemProc,instance);if(!proc){result=SZ_ERROR_MEM;strcpy(message,errorText(result));goto done;}
  choice=DialogBox(instance,MAKEINTRESOURCE(800),mainWindow,(DLGPROC)proc);FreeProcInstance(proc);
  if(choice!=1){result=SZ_ERROR_PROGRESS;strcpy(message,"Operation cancelled. No files were changed.");goto done;}
  if(strchr(fsDestination,':')||fsDestination[0]=='\\'||fsDestination[0]=='/')strcpy(target,fsDestination);
  else if(!folderJoin(saved,fsDestination,target)){strcpy(message,"The destination path is too long.");goto done;}
  if(!directoryPath(target,destination)){strcpy(message,"Choose an existing destination folder for multiple items.");goto done;}
  /* Reject a self/descendant destination or protected executable before
     publishing any item, including when a selected folder is the destination. */
  for(i=0;i<count;i++){
   if(!folderJoin(saved,names+13*i,source)||!folderJoin(destination,names+13*i,target)){strcpy(message,"A destination path is too long.");goto done;}
   if(sameName(source,target)||namePrefix(source,target)){strcpy(message,"The destination cannot be a selected item or inside a selected folder. No files were changed.");goto done;}
   if(protectsRunningManager(target)){result=SZ_ERROR_UNSUPPORTED;strcpy(message,"Close 7-Zip before replacing its executable. No files were changed.");goto done;}
   if(!stat(target,&info)&&(info.st_mode&S_IFMT)!=S_IFREG){result=SZ_ERROR_UNSUPPORTED;strcpy(message,"A destination folder already exists. Folder merging is not supported by this build. No files were changed.");goto done;}
  }
 }
 busy=1;cancelled=0;closePending=0;began=1;result=SZ_OK;message[0]=0;
 EnableWindow(fileList,FALSE);EnableWindow(pathEdit,FALSE);EnableWindow(openButton,FALSE);EnableWindow(createButton,FALSE);EnableWindow(cancelButton,TRUE);
 for(i=0;i<count;i++){
  if(cancelled){result=SZ_ERROR_PROGRESS;strcpy(message,"Operation cancelled.");break;}
  folderJoin(saved,names+13*i,source);target[0]=0;selectedOperation=operation;
  if(operation!=Z7_FS_DELETE){
   folderJoin(destination,names+13*i,target);
   if(!stat(target,&info)&&(info.st_mode&S_IFMT)==S_IFREG){
    sprintf(message,"Replace %s? Yes replaces this file, No skips it, Cancel stops the remaining items.",target);
    choice=fsTesting?fsReplaceTestChoice:MessageBox(mainWindow,message,fsTitle,MB_YESNOCANCEL|MB_ICONQUESTION|MB_DEFBUTTON2);
    message[0]=0;
    if(choice==IDNO){fsBatchSkipped++;continue;}
    if(choice!=IDYES){result=SZ_ERROR_PROGRESS;strcpy(message,"Operation cancelled.");break;}
    selectedOperation=operation==Z7_FS_MOVE?Z7_FS_MOVE_REPLACE:Z7_FS_COPY_REPLACE;
   }
  }
  sprintf(label,"%s item %u of %u",fsTitle,i+1,count);operationText=label;
  if(fsTesting&&fsBatchCancelItem==i+1){fsCancelTest=TRUE;fsBatchCancelItem=0;}
  result=Z7Win16_FileOperation(selectedOperation,source,target,&allocator,fsGuiProgress,&report);
  if(result!=SZ_OK){strcpy(message,report.message);break;}
  if(!warning[0]&&strstr(report.message,"WARNING:"))strcpy(warning,report.message);
  fsBatchCompleted++;
 }
 done:
 if(snapshot)ISzAlloc_Free(&allocator,snapshot);
 if(began){
  char detail[320];strncpy(detail,message,sizeof(detail)-1);detail[sizeof(detail)-1]=0;
  busy=0;operationText="Testing";EnableWindow(fileList,TRUE);EnableWindow(pathEdit,TRUE);EnableWindow(openButton,TRUE);EnableWindow(createButton,TRUE);EnableWindow(cancelButton,FALSE);
  loadDirectory(saved);sprintf(message,"%u of %u items completed; %u skipped. %s",fsBatchCompleted,count,fsBatchSkipped,detail[0]?detail:warning);
 }
 lastResult=result;SetWindowText(statusText,message);updateToolbarState();
 if(!fsTesting&&(result!=SZ_OK||warning[0]))MessageBox(mainWindow,message,fsTitle,MB_OK|MB_ICONEXCLAMATION);
 if(closePending)PostMessage(mainWindow,WM_CLOSE,0,0);
}
static void performFilesystemSingle(unsigned operation){
 char source[260],name[80],destination[260],full[260],saved[260],question[360];Z7FsReport report;SRes result;FARPROC proc;int choice;struct stat target;
 if(busy||!folderView)return;
 lastResult=SZ_ERROR_PARAM;source[0]=name[0]=destination[0]=0;strcpy(saved,folderPath);
 fsTitle=operation==Z7_FS_COPY?"Copy":operation==Z7_FS_MOVE?"Move":operation==Z7_FS_RENAME?"Rename":operation==Z7_FS_DELETE?"Delete":"New folder";
 if(operation!=Z7_FS_MKDIR&&!folderSelectedPath(source,name)){if(!fsTesting)MessageBox(mainWindow,"Select a file or folder. The parent entry cannot be changed.",fsTitle,MB_OK);return;}
 if(operation!=Z7_FS_COPY&&operation!=Z7_FS_MKDIR&&protectsRunningManager(source)){
  lastResult=SZ_ERROR_UNSUPPORTED;SetWindowText(statusText,"Close 7-Zip before moving, renaming or deleting its executable or installation folder.");if(!fsTesting)MessageBox(mainWindow,"Close 7-Zip before moving, renaming or deleting its executable or installation folder.",fsTitle,MB_OK|MB_ICONEXCLAMATION);return;
 }
 if(operation==Z7_FS_DELETE){
  sprintf(question,"Permanently delete %s? All contents of a selected folder will also be deleted. This operation cannot be undone.",name);
  if(!fsTesting&&MessageBox(mainWindow,question,fsTitle,MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDYES)return;
 }else{
  fsPrompt=(operation==Z7_FS_RENAME||operation==Z7_FS_MKDIR)?"New DOS 8.3 name:":"Destination folder or complete new path:";
  if(!fsTesting)strcpy(fsDestination,operation==Z7_FS_RENAME?name:operation==Z7_FS_MKDIR?"":folderPath);
  proc=MakeProcInstance((FARPROC)FilesystemProc,instance);if(!proc){lastResult=SZ_ERROR_MEM;return;}
  choice=DialogBox(instance,MAKEINTRESOURCE(800),mainWindow,(DLGPROC)proc);FreeProcInstance(proc);if(choice!=1)return;
  if(operation==Z7_FS_RENAME||operation==Z7_FS_MKDIR){if(!validDosComponent(fsDestination,(unsigned)strlen(fsDestination))||!folderJoin(saved,fsDestination,destination))goto invalid;}
  else{
   if(strchr(fsDestination,':')||fsDestination[0]=='\\'||fsDestination[0]=='/')strcpy(destination,fsDestination);
   else if(!folderJoin(saved,fsDestination,destination))goto invalid;
   if(directoryPath(destination,full)&&!folderJoin(full,name,destination))goto invalid;
  }
 }
 if((operation==Z7_FS_COPY||operation==Z7_FS_MOVE)&&!stat(destination,&target)&&(target.st_mode&S_IFMT)==S_IFREG){
  if(!_fullpath(full,destination,sizeof(full)))goto invalid;
  if(sameName(source,full)){lastResult=SZ_ERROR_PARAM;SetWindowText(statusText,"Source and destination refer to the same file.");return;}
  if(protectsRunningManager(full)){
   lastResult=SZ_ERROR_UNSUPPORTED;SetWindowText(statusText,"Close 7-Zip before replacing its executable.");if(!fsTesting)MessageBox(mainWindow,"Close 7-Zip before replacing its executable.",fsTitle,MB_OK|MB_ICONEXCLAMATION);return;
  }
  sprintf(question,"Replace %s? The existing file will be preserved until the new copy is complete.",destination);
  choice=fsTesting?fsReplaceTestChoice:MessageBox(mainWindow,question,fsTitle,MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
  if(choice!=IDYES){lastResult=SZ_ERROR_PROGRESS;SetWindowText(statusText,"Operation cancelled. The existing file was preserved.");return;}
  operation=operation==Z7_FS_MOVE?Z7_FS_MOVE_REPLACE:Z7_FS_COPY_REPLACE;
 }
 busy=1;cancelled=0;closePending=0;operationText=fsTitle;
 EnableWindow(fileList,FALSE);EnableWindow(pathEdit,FALSE);EnableWindow(openButton,FALSE);EnableWindow(createButton,FALSE);EnableWindow(cancelButton,TRUE);
 result=Z7Win16_FileOperation(operation,source,destination,&allocator,fsGuiProgress,&report);
 busy=0;EnableWindow(fileList,TRUE);EnableWindow(pathEdit,TRUE);EnableWindow(openButton,TRUE);EnableWindow(createButton,TRUE);EnableWindow(cancelButton,FALSE);
 loadDirectory(saved);lastResult=result;SetWindowText(statusText,report.message);
 if(!fsTesting&&(result!=SZ_OK||strstr(report.message,"WARNING:")))MessageBox(mainWindow,report.message,fsTitle,MB_OK|MB_ICONEXCLAMATION);
 if(closePending)PostMessage(mainWindow,WM_CLOSE,0,0);return;
invalid:
 lastResult=SZ_ERROR_UNSUPPORTED;SetWindowText(statusText,"Use a valid DOS 8.3 name or path.");if(!fsTesting)MessageBox(mainWindow,"Use a valid DOS 8.3 name or path.",fsTitle,MB_OK|MB_ICONEXCLAMATION);
}
static void performFilesystemBody(unsigned operation){
 long count;
 if(busy||!folderView)return;
 if(fileListMultiple&&(operation==Z7_FS_COPY||operation==Z7_FS_MOVE||operation==Z7_FS_DELETE)){
  count=SendMessage(fileList,LB_GETSELCOUNT,0,0);
  if(count>1){fsTitle=operation==Z7_FS_COPY?"Copy":operation==Z7_FS_MOVE?"Move":"Delete";performFilesystemBatch(operation,(unsigned)count);return;}
 }
 performFilesystemSingle(operation);
}
#ifdef __WATCOMC__
static unsigned filesystemGuiCs(void);
#pragma aux filesystemGuiCs = "mov ax,cs" value [ax] modify exact [ax];
#endif
static void performFilesystem(unsigned operation){
#ifdef __WATCOMC__
 unsigned segment;
#endif
 if(busy||!folderView)return;
 /* Dialogs and progress callbacks can compact real-mode code. Retain this
    command's return frames until its backend and window work have returned. */
#ifdef __WATCOMC__
 segment=filesystemGuiCs();
 if(!LockSegment(segment)){lastResult=SZ_ERROR_MEM;SetWindowText(statusText,errorText(lastResult));return;}
#endif
 performFilesystemBody(operation);
#ifdef __WATCOMC__
 UnlockSegment(segment);
#endif
}
#ifdef __WATCOMC__
#pragma code_seg()
#endif
