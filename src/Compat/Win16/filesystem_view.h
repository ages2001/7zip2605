/* Native DOS directory navigation for the Win16 File Manager.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_FILESYSTEM_VIEW_H
#define Z7_WIN16_FILESYSTEM_VIEW_H
#define ID_PARENT 118
#define ID_REFRESH 119
static char folderPath[260];
#ifdef __WATCOMC__
#pragma code_seg("FS_VIEW_TEXT")
#endif
static int folderRoot(const char *path){
 return path[0]&&path[1]==':'&&(path[2]=='\\'||path[2]=='/')&&!path[3]
     &&foldName((unsigned char)path[0])>='A'&&foldName((unsigned char)path[0])<='Z';
}
static int directoryPath(const char *path,char *full){
 struct stat info;unsigned length;
 if(!_fullpath(full,path,260))return 0;
 length=(unsigned)strlen(full);while(length>3&&(full[length-1]=='\\'||full[length-1]=='/'))full[--length]=0;
 if(!folderRoot(full)&&!validDosDestination(full))return 0;
 if(stat(full,&info)||(info.st_mode&S_IFMT)!=S_IFDIR)return 0;
 return 1;
}
static int directoryRow(const char *name,unsigned attributes,UInt32 length,unsigned wrdate,unsigned wrtime){
 char line[120],size[24],date[24],attr[8];LRESULT row;int kind;
 if(attributes&_A_SUBDIR)size[0]=0;else sizeText(length,size);
 dosDateText(wrdate,wrtime,date);attrText(attributes,attr);
 sprintf(line,"%s\t%s\t%s\t%s",name,size,date,attr);
 kind=!strcmp(name,"..")?Z7_ITEM_UP:((attributes&_A_SUBDIR)?Z7_ITEM_DIR:Z7_ITEM_FILE);
 row=SendMessage(fileList,LB_ADDSTRING,0,(LPARAM)(LPSTR)line);
 return row>=0&&SendMessage(fileList,LB_SETITEMDATA,(WPARAM)row,(LPARAM)kind)!=LB_ERR;
}
static void loadDirectory(const char *full){
 struct find_t entry;char pattern[260],message[100];unsigned code,pass;UInt32 count=0;size_t length;
 closeArchive();strcpy(folderPath,full);folderView=TRUE;SetWindowText(pathEdit,folderPath);
 if(!configureFileList(TRUE)){lastResult=SZ_ERROR_MEM;goto failed;}
 length=strlen(folderPath);if(length+5>=sizeof(pattern)){lastResult=SZ_ERROR_UNSUPPORTED;goto failed;}
 strcpy(pattern,folderPath);if(pattern[length-1]!='\\'&&pattern[length-1]!='/')pattern[length++]='\\';strcpy(pattern+length,"*.*");
 lastResult=SZ_OK;
 if(!folderRoot(folderPath)&&!directoryRow("..",_A_SUBDIR,0,0,0)){lastResult=SZ_ERROR_MEM;goto failed;}
 /* Two bounded-memory passes keep folders before files without retaining a
    second directory-entry array in scarce conventional memory. */
 for(pass=0;pass<2;pass++){
  code=_dos_findfirst(pattern,_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_SUBDIR|_A_ARCH,&entry);
  while(!code){
   if(strcmp(entry.name,".")&&strcmp(entry.name,"..")&&!!(entry.attrib&_A_SUBDIR)==!pass){
    if(!directoryRow(entry.name,entry.attrib,(UInt32)entry.size,entry.wr_date,entry.wr_time)){lastResult=SZ_ERROR_MEM;goto failed;}
    count++;
   }
   code=_dos_findnext(&entry);
  }
  if(code!=2&&code!=18){lastResult=SZ_ERROR_READ;goto failed;}
 }
 if(SendMessage(fileList,LB_GETCOUNT,0,0)>0)selectListItem(0);
 sprintf(message,"%lu items. Ctrl/Shift select multiple items; Ctrl+A selects all; Backspace goes up.",count);SetWindowText(statusText,message);updateToolbarState();return;
failed:
 SendMessage(fileList,LB_RESETCONTENT,0,0);folderView=FALSE;SetWindowText(statusText,errorText(lastResult));updateToolbarState();
}
static void openLocation(void){
 char path[260],full[260];
 if(busy)return;
 GetWindowText(pathEdit,path,sizeof(path));
 if(directoryPath(path,full))loadDirectory(full);else loadArchive();
}
static void parentLocation(void){
 char path[260],full[260],*slash;unsigned length;
 if(busy)return;
 GetWindowText(pathEdit,path,sizeof(path));if(!_fullpath(full,path,sizeof(full))||folderRoot(full))return;
 length=(unsigned)strlen(full);while(length>3&&(full[length-1]=='\\'||full[length-1]=='/'))full[--length]=0;
 slash=strrchr(full,'\\');if(!slash)slash=strrchr(full,'/');if(!slash)return;
 if(slash==full+2)slash[1]=0;else *slash=0;
 if(directoryPath(full,path))loadDirectory(path);
}
static void openFolderSelection(void){
 char name[80],path[260],*tab;long selected,length;size_t prefix;
 if(!folderView||busy)return;
 selected=singleListSelection();if(selected<0)return;
 length=SendMessage(fileList,LB_GETTEXTLEN,(WPARAM)selected,0);
 if(length<0||length>=sizeof(name))return;
 SendMessage(fileList,LB_GETTEXT,(WPARAM)selected,(LPARAM)(LPSTR)name);tab=strchr(name,'\t');if(tab)*tab=0;
 if(!strcmp(name,"..")){parentLocation();return;}
 if(!validDosComponent(name,(unsigned)strlen(name)))return;
 prefix=strlen(folderPath);if(prefix+strlen(name)+2>sizeof(path)){lastResult=SZ_ERROR_UNSUPPORTED;SetWindowText(statusText,errorText(lastResult));return;}
 strcpy(path,folderPath);if(path[prefix-1]!='\\'&&path[prefix-1]!='/')path[prefix++]='\\';strcpy(path+prefix,name);
 SetWindowText(pathEdit,path);openLocation();
}
/* Windows 3.0 list-box keyboard behavior differs between its modes. Manage
   Shift ranges and Ctrl+arrow / Ctrl+Space explicitly using the native caret.
   A fresh unmodified key or mouse click starts a new range. */
static int filesystemSelectionKey(WPARAM key){
 long caret,count,selected,start,end,i;int ctrl,shift;
 if(busy||(!folderView&&!opened)||!fileListMultiple)return 0;
 ctrl=GetKeyState(VK_CONTROL)<0;shift=GetKeyState(VK_SHIFT)<0;
 if(!shift)selectionAnchor=-1;
 if(ctrl&&key=='A'){
  SendMessage(fileList,LB_SETSEL,TRUE,-1);
  if(folderView&&!folderRoot(folderPath))SendMessage(fileList,LB_SETSEL,FALSE,0);
  updateToolbarState();return 1;
 }
 if(ctrl&&shift)return 0;
 if(!ctrl&&!shift)return 0;
 count=SendMessage(fileList,LB_GETCOUNT,0,0);caret=SendMessage(fileList,LB_GETCARETINDEX,0,0);
 if(count<=0||caret<0||caret>=count)return 0;
 if(ctrl&&key==VK_SPACE){
  selected=SendMessage(fileList,LB_GETSEL,(WPARAM)caret,0);
  if(selected>=0)SendMessage(fileList,LB_SETSEL,!selected,caret);
  updateToolbarState();return 1;
 }
 if(shift&&selectionAnchor<0)selectionAnchor=caret;
 if(key==VK_HOME)caret=0;
 else if(key==VK_END)caret=count-1;
 else if(key==VK_UP){if(caret)caret--;}
 else if(key==VK_DOWN){if(caret+1<count)caret++;}
 else return 0;
 if(shift){
  start=selectionAnchor<caret?selectionAnchor:caret;end=selectionAnchor>caret?selectionAnchor:caret;
  SendMessage(fileList,LB_SETSEL,FALSE,-1);
  for(i=start;i<=end;i++)SendMessage(fileList,LB_SETSEL,TRUE,i);
 }
 SendMessage(fileList,LB_SETCARETINDEX,(WPARAM)caret,FALSE);updateToolbarState();return 1;
}
#ifdef __WATCOMC__
#pragma code_seg()
#endif
#endif
