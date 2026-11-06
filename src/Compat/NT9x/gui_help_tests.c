/* Exercise the manual embedded in the actual installed File Manager. */
#define entry roundtrip_entry
#include "gui_roundtrip_tests.c"
#undef entry
void entry(void){char exe[MAX_PATH],body[80];DWORD version=GetVersion(),start;HANDLE process;HWND main,help,list;LRESULT total;
 report=CreateFileA("C:\\GUIHELP.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 lstrcpyA(exe,!(version&0x80000000UL)&&(version&255)==3?"C:\\7-Zip\\7zFM.exe":"C:\\Program Files\\7-Zip\\7zFM.exe");
 process=launch(exe,"C:\\",&main);PostMessageA(main,WM_COMMAND,960,0);help=waitWindow("Z7LegacyHelp",NULL);check(help!=NULL,"native English help opens from real menu");list=GetDlgItem(help,2);total=SendMessageA(list,LB_GETCOUNT,0,0);check(total>=50,"complete embedded manual topic index");check(SendMessageA(GetDlgItem(help,3),WM_GETTEXTLENGTH,0,0)>100,"selected manual topic contains content");
 check(text(help,1,"encryption"),"search the manual");check(SendMessageA(list,LB_GETCOUNT,0,0)>0&&SendMessageA(list,LB_GETCOUNT,0,0)<total,"search filters the full manual");
 check(text(help,1,"zz_no_such_topic_29817"),"search missing topic");SendMessageA(GetDlgItem(help,3),WM_GETTEXT,sizeof(body),(LPARAM)body);check(SendMessageA(list,LB_GETCOUNT,0,0)==0&&!lstrcmpA(body,"No matching help topics."),"missing search has explicit English feedback");
 check(text(help,1,"")&&SendMessageA(list,LB_GETCOUNT,0,0)==total,"clearing search restores all topics");PostMessageA(help,WM_CLOSE,0,0);start=GetTickCount();while(IsWindow(help)&&GetTickCount()-start<10000)Sleep(20);check(!IsWindow(help),"manual closes normally");stop(process,main);logLine("PASS ALL");CloseHandle(report);ExitProcess(0);
}
