/* Regression for user-edited uninstall commands and mixed-ownership groups. */
#define WinMain setup_integration_main
#include "setup_integration_tests.c"
#undef WinMain
int WINAPI WinMain(HINSTANCE a,HINSTANCE b,LPSTR c,int d){char text[4096];
 report=CreateFileA("C:\\OWN24.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)return 2;
 if(!isNt3()){line("FAIL: this fixture requires NT 3.x");CloseHandle(report);return 2;}
 checkItems(TRUE);
 check(groupText(text,sizeof(text),"[ShowGroup(\"7-Zip 26.05\",1)][DeleteItem(\"Uninstall 7-Zip\")][AddItem(\"C:\\7-Zip\\7z.exe\",\"Uninstall 7-Zip\",\"C:\\7-Zip\\Uninst.exe\",0)]"),"simulate edited uninstall command retaining original icon");
 check(Z7SetupCreateShortcuts("C:\\7-Zip\\7zFM.exe","C:\\7-Zip")==ERROR_ALREADY_EXISTS,"installer rejects unrelated uninstall command despite matching icon");
 check(groupText(text,sizeof(text),NULL)&&strstr(text,"\"7-Zip File Manager\",\"C:\\7-Zip\\7zFM.exe\",")!=NULL,"preflight conflict preserves original File Manager item");
 check(Z7SetupRemoveShortcuts("C:\\7-Zip\\7zFM.exe")==0,"mixed-ownership removal succeeds");
 check(groupText(text,sizeof(text),NULL)&&!strstr(text,"\"7-Zip File Manager\",")&&strstr(text,"\"Uninstall 7-Zip\",\"C:\\7-Zip\\7z.exe\",")!=NULL,"removal preserves edited item and its group");line(text);
 check(groupText(text,sizeof(text),"[ShowGroup(\"7-Zip 26.05\",1)][DeleteItem(\"Uninstall 7-Zip\")]"),"fixture removes its own edited item");
 check(Z7SetupCreateShortcuts("C:\\7-Zip\\7zFM.exe","C:\\7-Zip")==0,"restore installed shortcuts");checkItems(TRUE);check(failures==0,"ALL");CloseHandle(report);return failures?1:0;
}
