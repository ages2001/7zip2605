/* Native, searchable English manual viewer; requires no HTML Help runtime.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "help_topics.h"
static HWND help_window,topic_list,topic_text,search_box;
static unsigned topic_map[sizeof(topics)/sizeof(topics[0])];
static HINSTANCE instance;
static const char *body(unsigned index) {
 HRSRC resource=FindResourceA(instance,MAKEINTRESOURCEA(topics[index].id),RT_RCDATA);
 HGLOBAL loaded=resource?LoadResource(instance,resource):NULL;
 return loaded?(const char*)LockResource(loaded):NULL;
}
static char lower(char c) {return c>='A'&&c<='Z'?(char)(c+'a'-'A'):c;}
static BOOL contains(const char *text,const char *term) {
 unsigned i;
 if(!text)return FALSE;
 if(!*term)return TRUE;
 for(;*text;++text){for(i=0;term[i]&&text[i]&&lower(term[i])==lower(text[i]);++i){}if(!term[i])return TRUE;}
 return FALSE;
}
static void show_selection(void) {
 LRESULT selected=SendMessageA(topic_list,LB_GETCURSEL,0,0);
 if(selected!=LB_ERR){const char *text=body(topic_map[selected]);SetWindowTextA(topic_text,text?text:"The help topic could not be loaded.");}
}
static void filter(void) {
 char term[256];unsigned i,n=0;
 GetWindowTextA(search_box,term,sizeof(term));SendMessageA(topic_list,LB_RESETCONTENT,0,0);
 for(i=0;i<sizeof(topics)/sizeof(topics[0]);i++)if(contains(topics[i].title,term)||contains(body(i),term)){
  topic_map[n++]=i;SendMessageA(topic_list,LB_ADDSTRING,0,(LPARAM)topics[i].title);
 }
 if(n){SendMessageA(topic_list,LB_SETCURSEL,0,0);show_selection();}
 else SetWindowTextA(topic_text,"No matching help topics.");
}
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
 switch(message){
 case WM_CREATE:
  CreateWindowA("STATIC","Search all topics:",WS_CHILD|WS_VISIBLE,8,10,120,20,window,0,instance,0);
  search_box=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL,130,6,300,24,window,(HMENU)1,instance,0);
  topic_list=CreateWindowExA(WS_EX_CLIENTEDGE,"LISTBOX","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|LBS_NOINTEGRALHEIGHT,8,38,210,400,window,(HMENU)2,instance,0);
  topic_text=CreateWindowExA(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,226,38,470,400,window,(HMENU)3,instance,0);
  if(!search_box||!topic_list||!topic_text)return -1;
  SendMessageA(search_box,EM_LIMITTEXT,255,0);SendMessageA(topic_text,EM_LIMITTEXT,60000,0);
  filter();return 0;
 case WM_SIZE:{int width=LOWORD(lp),height=HIWORD(lp);int split=width/3;
  MoveWindow(search_box,130,6,width>146?width-138:8,24,TRUE);
  MoveWindow(topic_list,8,38,split>16?split-16:8,height>46?height-46:8,TRUE);
  MoveWindow(topic_text,split,38,width>split+8?width-split-8:8,height>46?height-46:8,TRUE);return 0;}
 case WM_COMMAND:
  if(LOWORD(wp)==1&&HIWORD(wp)==EN_CHANGE){filter();return 0;}
  if(LOWORD(wp)==2&&HIWORD(wp)==LBN_SELCHANGE){show_selection();return 0;}
  break;
 case WM_CLOSE:DestroyWindow(window);return 0;
 case WM_DESTROY:help_window=NULL;return 0;
 }
 return DefWindowProcA(window,message,wp,lp);
}
void LegacyShowHelp(const char *topic) {
 unsigned i;
 if(!help_window){WNDCLASSA wc={0};instance=GetModuleHandleA(NULL);wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.hCursor=LoadCursorA(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName="Z7LegacyHelp";
  if(!RegisterClassA(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){MessageBoxA(GetActiveWindow(),"The help window could not be registered.","7-Zip Help",MB_OK|MB_ICONERROR);return;}
  help_window=CreateWindowExA(0,wc.lpszClassName,"7-Zip 26.05 Help",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,720,500,GetActiveWindow(),NULL,instance,NULL);
  if(!help_window){MessageBoxA(GetActiveWindow(),"The help window could not be created.","7-Zip Help",MB_OK|MB_ICONERROR);return;}
 }
 SetWindowTextA(search_box,"");
 if(topic)for(i=0;i<sizeof(topics)/sizeof(topics[0]);i++)if(lstrcmpiA(topic,topics[i].path)==0){SendMessageA(topic_list,LB_SETCURSEL,i,0);show_selection();break;}
 ShowWindow(help_window,SW_SHOWNORMAL);SetActiveWindow(help_window);
}
