/* Inspect the actual NT3.1 File Manager C:\ view and saved column settings.
   Run in a disposable guest with File Manager already open. */
#include <windows.h>
#include <commctrl.h>
static HANDLE report;static unsigned failed;
static void line(char*s){DWORD n;WriteFile(report,s,lstrlenA(s),&n,0);}
static BOOL CALLBACK child(HWND w,LPARAM unused){char cls[80],s[128];int i;LRESULT width;GetClassNameA(w,cls,80);if(lstrcmpiA(cls,"SysListView32"))return TRUE;for(i=0;i<32;i++){width=SendMessageA(w,LVM_GETCOLUMNWIDTH,i,0);if(!width)break;wsprintfA(s,"logical column %d width %ld\r\n",i,width);line(s);}wsprintfA(s,"visible columns %d\r\n",i);line(s);if(i!=7)failed++;return TRUE;}
void entry(void){HKEY k;DWORD i,size,n,type,j;static BYTE data[4096];char name[256],s[256];LONG result;report=CreateFileA("C:\\COLSAVE.TXT",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);EnumChildWindows(FindWindowA("7-Zip::FM",0),child,0);result=RegOpenKeyExA(HKEY_CURRENT_USER,"Software\\7-Zip\\FM\\Columns",0,KEY_READ,&k);if(!result){for(i=0;;i++){size=sizeof(data);n=sizeof(name);result=RegEnumValueA(k,i,name,&n,0,&type,data,&size);if(result==ERROR_NO_MORE_ITEMS)break;if(result){failed++;break;}wsprintfA(s,"saved view %s type %lu bytes %lu\r\n",name,type,size);line(s);for(j=0;j+4<=size;j+=4){wsprintfA(s,"%lu ",*(DWORD*)(data+j));line(s);}line("\r\n");}RegCloseKey(k);}else failed++;line(failed?"FAIL ALL\r\n":"PASS ALL\r\n");FlushFileBuffers(report);CloseHandle(report);ExitProcess(failed?1:0);}
