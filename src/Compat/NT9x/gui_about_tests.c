/* Verify the port credit in the actual installed File Manager dialog. */
#define entry roundtrip_entry
#include "gui_roundtrip_tests.c"
#undef entry
static BOOL CALLBACK findMainToolbar(HWND w,LPARAM p){char cls[80];GetClassNameA(w,cls,sizeof(cls));if(!lstrcmpiA(cls,"ToolbarWindow32")&&SendMessageA(w,TB_BUTTONCOUNT,0,0)>=7){*(HWND*)p=w;return FALSE;}return TRUE;}
static void toolbarPixels(HWND main){HWND toolbar=NULL;HDC dc;COLORREF pixel;char detail[96];EnumChildWindows(main,findMainToolbar,(LPARAM)&toolbar);check(toolbar!=NULL,"find actual application toolbar");
 {RECT bounds;check(GetClientRect(toolbar,&bounds)&&bounds.bottom-bounds.top>=40,"default toolbar reserves space for labels below the icons");}
 /* TB_GETITEMRECT is WM_USER-based and cannot marshal a pointer into another
    process on these systems. Sample the blank inset of the first button. */
 Sleep(300);dc=GetDC(toolbar);pixel=GetPixel(dc,6,4);ReleaseDC(toolbar,dc);wsprintfA(detail,"Toolbar inset pixel=%08lX button face=%08lX",pixel,GetSysColor(COLOR_BTNFACE));logLine(detail);check(pixel==GetSysColor(COLOR_BTNFACE),"actual toolbar icon corner matches button face, with no white rectangle");}

static void originalLogo(HWND about){
 HWND logo=GetDlgItem(about,104);RECT r;HDC dc;COLORREF corner,black;
 check(logo!=NULL,"original logo control exists");GetClientRect(logo,&r);
 check(r.right==110&&r.bottom==63,"original logo keeps its 110 by 63 pixel dimensions");
 Sleep(300);dc=GetDC(logo);corner=GetPixel(dc,0,0);black=GetPixel(dc,1,1);ReleaseDC(logo,dc);
 check(black==RGB(0,0,0),"original logo black outline is drawn at native coordinates");
 dc=GetDC(about);GetWindowRect(logo,&r);MapWindowPoints(NULL,about,(POINT*)&r,2);
 check(corner==GetPixel(dc,r.left-1,r.top),"original logo transparent corner matches the dialog background");ReleaseDC(about,dc);
}
void entry(void){char exe[MAX_PATH],body[256];DWORD version=GetVersion();HANDLE process;HWND main,about,credit;RECT r,client;
 report=CreateFileA("C:\\ABOUT24.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 lstrcpyA(exe,!(version&0x80000000UL)&&(version&255)==3?"C:\\7-Zip\\7zFM.exe":"C:\\Program Files\\7-Zip\\7zFM.exe");
#ifdef Z7_ABOUT_TEST_PATH
 lstrcpyA(exe,Z7_ABOUT_TEST_PATH);
#endif
 process=launch(exe,"C:\\",&main);toolbarPixels(main);PostMessageA(main,WM_COMMAND,961,0);about=waitWindow("#32770","About 7-Zip");check(about!=NULL,"About opens from the actual menu");originalLogo(about);credit=GetDlgItem(about,103);check(credit!=NULL,"port credit control exists");GetWindowTextA(credit,body,sizeof(body));check(!lstrcmpA(body,"Ported to Windows NT 3.1/3.5x/4 and\nWindows 95/98/ME by ages2001"),"exact English port credit");GetWindowRect(credit,&r);MapWindowPoints(NULL,about,(POINT*)&r,2);GetClientRect(about,&client);check(r.left>=0&&r.top>=0&&r.right<=client.right&&r.bottom<=client.bottom,"credit fits inside dialog");GetWindowTextA(GetDlgItem(about,101),body,sizeof(body));check(!lstrcmpA(body,"7-Zip 26.05 (x86-32)"),"exact x86-32 version label");Sleep(3000);PostMessageA(about,WM_COMMAND,IDOK,0);Sleep(500);stop(process,main);logLine("PASS ALL");CloseHandle(report);ExitProcess(0);
}
