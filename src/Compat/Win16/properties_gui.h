/* Read-only file/archive properties for the native browser.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_PROPERTIES_GUI_H
#define Z7_WIN16_PROPERTIES_GUI_H
#define ID_INFO 123
static char *propertiesText;
static BOOL propertiesTesting,propertiesValid;
static const char *propertiesExpected;
BOOL FAR PASCAL __export PropertiesProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam){
 (void)lParam;
 switch(message){
 case WM_INITDIALOG:
  SetDlgItemText(window,901,propertiesText);
  if(propertiesTesting){
   char *visible=(char *)ISzAlloc_Alloc(&allocator,1024);propertiesValid=FALSE;
   if(visible){GetDlgItemText(window,901,visible,1024);propertiesValid=!strcmp(visible,propertiesText)&&(!propertiesExpected||strstr(visible,propertiesExpected)!=NULL);ISzAlloc_Free(&allocator,visible);}
   PostMessage(window,WM_COMMAND,902,0);PostMessage(window,WM_COMMAND,IDOK,0);
  }return TRUE;
 case WM_COMMAND:
  if(wParam==902){HWND edit=GetDlgItem(window,901);SendMessage(edit,EM_SETSEL,0,MAKELONG(0,32767));SendMessage(edit,WM_COPY,0,0);return TRUE;}
  if(wParam==IDOK||wParam==IDCANCEL){EndDialog(window,1);return TRUE;}break;
 case WM_CLOSE:EndDialog(window,0);return TRUE;
 }
 return FALSE;
}
static void propertiesNtfsTime(const CNtfsFileTime *time,char *out){
 static const unsigned monthDays[12]={31,28,31,30,31,30,31,31,30,31,30,31};
 UInt64 seconds=(((UInt64)time->High<<32)|time->Low)/10000000UL;UInt32 days=(UInt32)(seconds/86400UL),within=(UInt32)(seconds%86400UL);unsigned year,month,span,leap;
 year=(unsigned)(1601UL+400UL*(days/146097UL));days%=146097UL;
 for(;;){leap=!(year%4)&&((year%100)||!(year%400));span=365+leap;if(days<span)break;days-=span;year++;}
 for(month=0;month<11;month++){span=monthDays[month]+(month==1?leap:0);if(days<span)break;days-=span;}
 sprintf(out,"%04u-%02u-%02u %02u:%02u:%02u",year,month+1,(unsigned)days+1,(unsigned)(within/3600),(unsigned)(within/60%60),(unsigned)(within%60));
}
static void showProperties(void){
 char path[260],name[260],size[24],date[40],crc[24],attributes[24];struct stat info;long selected;FARPROC proc;int result;
 if(busy||propertiesText||(!opened&&!folderView))return;
 if(fileListMultiple&&SendMessage(fileList,LB_GETSELCOUNT,0,0)>1){lastResult=SZ_ERROR_PARAM;SetWindowText(statusText,"Select one item to view its detailed properties.");return;}
 propertiesText=(char *)ISzAlloc_Alloc(&allocator,1024);if(!propertiesText){lastResult=SZ_ERROR_MEM;SetWindowText(statusText,errorText(lastResult));return;}
 GetWindowText(pathEdit,path,sizeof(path));selected=singleListSelection();
 if(opened&&selected>=0&&(UInt32)selected<archive.NumFiles){
  nameText((UInt32)selected,name,sizeof(name));sizeText(SzArEx_GetFileSize(&archive,(UInt32)selected),size);strcpy(date,"Not stored");strcpy(crc,"Not stored");strcpy(attributes,"Not stored");
  if(SzBitWithVals_Check(&archive.MTime,selected))propertiesNtfsTime(archive.MTime.Vals+selected,date);
  if(SzBitWithVals_Check(&archive.CRCs,selected))sprintf(crc,"%08lX",(unsigned long)archive.CRCs.Vals[selected]);
  if(SzBitWithVals_Check(&archive.Attribs,selected))sprintf(attributes,"0x%08lX",(unsigned long)archive.Attribs.Vals[selected]);
  sprintf(propertiesText,"Archive: %s\r\nName: %s\r\nType: %s\r\nUnpacked size: %s bytes\r\nCRC32: %s\r\nModified (UTC): %s\r\nAttributes: %s",path,name,SzArEx_IsDir(&archive,selected)?"Folder":"File",size,crc,date,attributes);
 }else if(opened){
  sizeText(archive.NumFiles?archive.UnpackPositions[archive.NumFiles]:0,size);sprintf(propertiesText,"Archive: %s\r\nItems: %lu\r\nTotal unpacked size: %s bytes",path,(unsigned long)archive.NumFiles,size);
 }else{
  unsigned attrs;struct find_t found;
  if(!folderSelectedPath(path,name)){strcpy(path,folderPath);strcpy(name,folderPath);}
  if(stat(path,&info)||_dos_getfileattr(path,&attrs)){lastResult=SZ_ERROR_READ;goto failed;}
  sizeText((UInt64)info.st_size,size);strcpy(date,"Not available");
  if(!_dos_findfirst(path,0x37,&found)){
   unsigned year=1980+(found.wr_date>>9),month=(found.wr_date>>5)&15,day=found.wr_date&31;
   unsigned hour=found.wr_time>>11,minute=(found.wr_time>>5)&63,second=(found.wr_time&31)*2;
   if(month>=1&&month<=12&&day>=1&&day<=31&&hour<24&&minute<60&&second<60)
    sprintf(date,"%04u-%02u-%02u %02u:%02u:%02u",year,month,day,hour,minute,second);
  }
  sprintf(propertiesText,"Path: %s\r\nType: %s\r\nSize: %s bytes\r\nModified (local): %s\r\nAttributes: 0x%02X",path,(info.st_mode&S_IFMT)==S_IFDIR?"Folder":"File",size,date,attrs);
 }
 proc=MakeProcInstance((FARPROC)PropertiesProc,instance);if(!proc){lastResult=SZ_ERROR_MEM;goto failed;}
 result=DialogBox(instance,MAKEINTRESOURCE(900),mainWindow,(DLGPROC)proc);FreeProcInstance(proc);lastResult=result<0?SZ_ERROR_MEM:SZ_OK;
failed:
 ISzAlloc_Free(&allocator,propertiesText);propertiesText=NULL;if(lastResult!=SZ_OK)SetWindowText(statusText,errorText(lastResult));
}
#endif
