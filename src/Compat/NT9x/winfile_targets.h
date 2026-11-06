/* Resolve legacy WinFile drop destinations without depending on its private
   directory-entry structures. Copyright (c) 2026. LGPL-2.1-or-later. */
typedef struct { FILETIME time; DWORD size; BYTE attributes; WCHAR path[260]; } FileSelW;
typedef struct { FILETIME time; DWORD size; BYTE attributes; CHAR path[260]; } FileSelA;

static HWND dropPaneAt(POINT point,HWND *document,HWND *list,BOOL *tree)
{
  HWND window=WindowFromPoint(point),parent,pane=NULL;
  CHAR name[32];
  *document=NULL;*list=NULL;*tree=FALSE;
  if(!window||!IsChild(frame,window))return NULL;
  while(window&&window!=frame){
    GetClassNameA(window,name,sizeof(name));
    if(!lstrcmpiA(name,"DirTree")){pane=window;*tree=TRUE;}
    else if(!lstrcmpiA(name,"WFS_Dir")){pane=window;*tree=FALSE;}
    parent=GetParent(window);
    if(parent){
      GetClassNameA(parent,name,sizeof(name));
      if(!lstrcmpiA(name,"MDIClient")){*document=window;break;}
    }
    window=parent;
  }
  if(!pane||!*document)return NULL;
  *list=GetDlgItem(pane,*tree?3:6);
  return *list?pane:NULL;
}

/* LB_ITEMFROMPOINT was added after NT 3.1. Inspect visible item rectangles,
   including the old multicolumn list layout, using the original list API. */
static int listHitAt(HWND list,POINT point)
{
  RECT client,item;int index,count;BOOL columns;
  ScreenToClient(list,&point);GetClientRect(list,&client);
  if(!PtInRect(&client,point))return -1;
  index=(int)SendMessageA(list,LB_GETTOPINDEX,0,0);
  count=(int)SendMessageA(list,LB_GETCOUNT,0,0);
#ifdef Z7_WF_TRACE
  {CHAR line[180];wsprintfA(line,"Hit point %ld,%ld client %ld,%ld top=%d count=%d",point.x,point.y,client.right,client.bottom,index,count);trace(line);}
#endif
  columns=(GetWindowLongA(list,GWL_STYLE)&LBS_MULTICOLUMN)!=0;
  if(index<0||count<0)return -1;
  for(;index<count;index++){
    LRESULT result=SendMessageA(list,LB_GETITEMRECT,index,(LPARAM)&item);
#ifdef Z7_WF_TRACE
    {CHAR line[180];wsprintfA(line,"Item %d result=%ld rect=%ld,%ld,%ld,%ld columns=%d",index,result,item.left,item.top,item.right,item.bottom,columns);trace(line);}
#endif
    if(result==LB_ERR)break;
    if(item.left>=client.right||(!columns&&item.top>=client.bottom))break;
    if(PtInRect(&item,point))return index;
  }
  return -1;
}

static BOOL dropPointValid(POINTL point)
{
  POINT p;HWND document,list;BOOL tree;RECT client;
  p.x=point.x;p.y=point.y;
  if(!dropPaneAt(p,&document,&list,&tree))return FALSE;
  if(tree)return listHitAt(list,p)>=0;
  ScreenToClient(list,&p);GetClientRect(list,&client);
  return PtInRect(&client,p);
}

static void directorySelectionChanged(HWND list)
{
  SendMessageA(GetParent(list),WM_COMMAND,
      MAKEWPARAM(GetDlgCtrlID(list),LBN_SELCHANGE),(LPARAM)list);
}

/* The public extension selection API intentionally omits the parent row.
   WinFile's original FS_GETSELECTION query with type 1 includes it and returns
   a caller-owned LocalAlloc string. Ask the pane instead of guessing that
   item zero is a parent or reading its private owner-draw structure. */
static BOOL parentSelection(HWND pane,WCHAR *out)
{
  HLOCAL selection;SIZE_T bytes;unsigned n,first,last;BOOL directory=FALSE,ok=FALSE;
  WCHAR path[260];
  selection=(HLOCAL)SendMessageA(pane,WM_USER+0x102,1,(LPARAM)&directory);
  if(!selection)return FALSE;
  bytes=LocalSize(selection);
  if(directory&&bytes){
    if(IsWindowUnicode(pane)){
      const WCHAR *text=(const WCHAR *)selection;
      for(n=0;n<260&&n<bytes/sizeof(WCHAR)&&text[n];n++);
      if(n<260&&n<bytes/sizeof(WCHAR)){
        for(first=0;first<=n;first++)path[first]=text[first];ok=TRUE;
      }
    }else{
      const CHAR *text=(const CHAR *)selection;
      for(n=0;n<260&&n<bytes&&text[n];n++);
      if(n<260&&n<bytes)ok=MultiByteToWideChar(CP_ACP,0,text,-1,path,260)!=0;
    }
  }
  if(ok){
    first=0;last=lstrlenW(path);
    if(last>=2&&path[0]=='"'&&path[last-1]=='"'){first++;last--;}
    n=0;while(first<last)out[n++]=path[first++];out[n]=0;ok=n!=0;
  }
#ifdef Z7_WF_TRACE
  {CHAR line[300];wsprintfA(line,"Parent selection bytes=%lu directory=%d ok=%d",(DWORD)bytes,directory,ok);trace(line);
   if(ok){WideCharToMultiByte(CP_ACP,0,out,-1,line,sizeof(line),NULL,NULL);trace(line);}}
#endif
  LocalFree(selection);return ok;
}

/* WinFile's extension API exposes selected entries only. Preserve every
   selected index and the caret/scroll positions around the synchronous query.
   Never interpret the owner-draw list's opaque LB_GETTEXT item data. */
static BOOL directoryItem(HWND list,int index,WCHAR *out)
{
  int count,got,i,caret,anchor,top,*selected=NULL;BOOL ok=FALSE;
  FileSelW wide;FileSelA ansi;
  count=(int)SendMessageA(list,LB_GETSELCOUNT,0,0);
  if(count<0||count>0x100000)return FALSE;
  if(count){
    selected=HeapAlloc(GetProcessHeap(),0,count*sizeof(int));
    if(!selected)return FALSE;
    got=(int)SendMessageA(list,LB_GETSELITEMS,count,(LPARAM)selected);
    if(got!=count){HeapFree(GetProcessHeap(),0,selected);return FALSE;}
  }
  caret=(int)SendMessageA(list,LB_GETCARETINDEX,0,0);
  anchor=(int)SendMessageA(list,LB_GETANCHORINDEX,0,0);
  top=(int)SendMessageA(list,LB_GETTOPINDEX,0,0);
  SendMessageA(list,WM_SETREDRAW,FALSE,0);
  if(SendMessageA(list,LB_SETSEL,FALSE,-1)!=LB_ERR&&
      SendMessageA(list,LB_SETSEL,TRUE,index)!=LB_ERR){
    directorySelectionChanged(list);
    if(index==0)ok=parentSelection(GetParent(list),out);
    ZeroMemory(&wide,sizeof(wide));
    if(!ok)SendMessageW(frame,WM_USER+0x215,0,(LPARAM)&wide);
    wide.path[259]=0;
    if(wide.path[0]){lstrcpyW(out,wide.path);ok=TRUE;}
    else if(!ok){
      ZeroMemory(&ansi,sizeof(ansi));
      SendMessageA(frame,WM_USER+0x205,0,(LPARAM)&ansi);
      ansi.path[259]=0;
      if(ansi.path[0])ok=MultiByteToWideChar(CP_ACP,0,ansi.path,-1,out,260)!=0;
    }
  }
  SendMessageA(list,LB_SETSEL,FALSE,-1);
  for(i=0;i<count;i++)SendMessageA(list,LB_SETSEL,TRUE,selected[i]);
  if(caret>=0)SendMessageA(list,LB_SETCARETINDEX,caret,FALSE);
  if(anchor>=0)SendMessageA(list,LB_SETANCHORINDEX,anchor,0);
  if(top>=0)SendMessageA(list,LB_SETTOPINDEX,top,0);
  directorySelectionChanged(list);
  SendMessageA(list,WM_SETREDRAW,TRUE,0);InvalidateRect(list,NULL,FALSE);
  if(selected)HeapFree(GetProcessHeap(),0,selected);
  return ok;
}

static BOOL destinationAt(POINTL point,WCHAR *out)
{
  POINT p;HWND pane,document,list;BOOL tree;int index;DWORD attributes;
  DriveW drive;WCHAR item[260];
  p.x=point.x;p.y=point.y;
  pane=dropPaneAt(p,&document,&list,&tree);
  if(!pane)return FALSE;
  SendMessageA(GetParent(document),WM_MDIACTIVATE,(WPARAM)document,0);
  index=listHitAt(list,p);
#ifdef Z7_WF_TRACE
  {CHAR line[100];wsprintfA(line,"Hit item %d tree=%d",index,tree);trace(line);}
#endif
  if(tree){
    if(index<0)return FALSE;
    ZeroMemory(item,sizeof(item));
    /* TC_GETDIR is the original WinFile tree-control query. It takes a
       list index and writes a full path, without changing the selection.
       This DLL runs inside WinFile; no cross-process pointer is passed. */
    if(IsWindowUnicode(pane))SendMessageW(pane,0x948,index,(LPARAM)item);
    else{
      CHAR ansi[260];ZeroMemory(ansi,sizeof(ansi));
      SendMessageA(pane,0x948,index,(LPARAM)ansi);ansi[259]=0;
      if(!MultiByteToWideChar(CP_ACP,0,ansi,-1,item,260))return FALSE;
    }
    item[259]=0;
  }else{
    ZeroMemory(&drive,sizeof(drive));
    SendMessageW(frame,FM_GETDRIVEINFOW,0,(LPARAM)&drive);drive.path[259]=0;
    if(!drive.path[0]){
      DriveA ansi;ZeroMemory(&ansi,sizeof(ansi));
      SendMessageA(frame,WM_USER+0x201,0,(LPARAM)&ansi);ansi.path[259]=0;
      if(!MultiByteToWideChar(CP_ACP,0,ansi.path,-1,drive.path,260))return FALSE;
    }
    if(index>=0){
      if(!directoryItem(list,index,item))return FALSE;
#ifdef Z7_WF_TRACE
      {CHAR line[260];WideCharToMultiByte(CP_ACP,0,item,-1,line,260,NULL,NULL);trace(line);}
#endif
      attributes=GetFileAttributesW(item);
      if(attributes==INVALID_FILE_ATTRIBUTES)return FALSE;
      if(!(attributes&FILE_ATTRIBUTE_DIRECTORY))lstrcpyW(item,drive.path);
    }else lstrcpyW(item,drive.path);
  }
  if(item[0]&&item[1]==':'&&!item[2]){item[2]='\\';item[3]=0;}
  attributes=GetFileAttributesW(item);
  if(!item[0]||attributes==INVALID_FILE_ATTRIBUTES||!(attributes&FILE_ATTRIBUTE_DIRECTORY))return FALSE;
  lstrcpyW(out,item);return TRUE;
}

static void refreshDestination(POINTL point)
{
  POINT p;HWND document,list,treeList=NULL;BOOL tree;int row=-1,selected,top;
  p.x=point.x;p.y=point.y;
  if(dropPaneAt(p,&document,&list,&tree)){
    HWND pane=GetDlgItem(document,5);
    if(pane)treeList=GetDlgItem(pane,3);
    if(treeList){
      selected=(int)SendMessageA(treeList,LB_GETCURSEL,0,0);
      top=(int)SendMessageA(treeList,LB_GETTOPINDEX,0,0);
      if(selected>=top&&top>=0)row=selected-top;
    }
  }
  SendMessageA(frame,FM_REFRESH_WINDOWS,0,0);
  /* Refresh can insert sibling nodes before the current directory. Preserve
     its visible row, rather than its obsolete pre-refresh list index. */
  if(row>=0&&IsWindow(treeList)){
    selected=(int)SendMessageA(treeList,LB_GETCURSEL,0,0);
    if(selected>=0)SendMessageA(treeList,LB_SETTOPINDEX,selected>row?selected-row:0,0);
  }
}
