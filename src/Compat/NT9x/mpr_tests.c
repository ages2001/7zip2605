/* Deterministic provider-tree tests for the NT 3.1 MPR fallback.
   These exercise resource traversal and packing, not a live remote server.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnetwk.h>
void *memset(void *p,int c,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)p)[i]=c;return p;}
void *memcpy(void *d,const void *s,unsigned n){unsigned i;for(i=0;i<n;i++)((BYTE*)d)[i]=((const BYTE*)s)[i];return d;}
static HANDLE report;static unsigned opened,closed,growth;static DWORD networkError;
static void check(BOOL b,const char *name){DWORD n;const char *s=b?"PASS ":"FAIL ";WriteFile(report,s,lstrlenA(s),&n,NULL);WriteFile(report,name,lstrlenA(name),&n,NULL);WriteFile(report,"\r\n",2,&n,NULL);FlushFileBuffers(report);if(!b){CloseHandle(report);ExitProcess(1);}}
typedef struct Enumerator{int level;BOOL used;} Enumerator;
DWORD WINAPI TestNetOpen(DWORD scope,DWORD type,DWORD usage,LPNETRESOURCEW parent,LPHANDLE out){Enumerator *e;(void)scope;(void)type;(void)usage;if(networkError)return networkError;e=HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,sizeof(*e));if(!e)return ERROR_NOT_ENOUGH_MEMORY;e->level=!parent?0:!lstrcmpW(parent->lpRemoteName,L"Provider")?1:!lstrcmpW(parent->lpRemoteName,L"Domain")?2:!lstrcmpW(parent->lpRemoteName,L"\\\\server")?3:4;*out=e;opened++;return NO_ERROR;}
DWORD WINAPI TestNetEnum(HANDLE handle,LPDWORD count,LPVOID buffer,LPDWORD bytes){
 Enumerator *e=handle;NETRESOURCEW *r=buffer;static WCHAR *names[]={L"Provider",L"Domain",L"\\\\server",L"\\\\server\\share"};
 if(e->used||e->level>3)return ERROR_NO_MORE_ITEMS;if(*bytes<20000){*bytes=20000;growth++;return ERROR_MORE_DATA;}
 ZeroMemory(r,sizeof(*r));r->dwScope=RESOURCE_GLOBALNET;r->dwType=e->level==3?RESOURCETYPE_DISK:RESOURCETYPE_ANY;r->dwDisplayType=e->level==0?RESOURCEDISPLAYTYPE_NETWORK:e->level==1?RESOURCEDISPLAYTYPE_DOMAIN:e->level==2?RESOURCEDISPLAYTYPE_SERVER:RESOURCEDISPLAYTYPE_SHARE;
 r->dwUsage=e->level==3?RESOURCEUSAGE_CONNECTABLE:RESOURCEUSAGE_CONTAINER;r->lpRemoteName=names[e->level];r->lpProvider=L"Test Provider";r->lpComment=L"Resource comment";e->used=TRUE;*count=1;return NO_ERROR;
}
DWORD WINAPI TestNetClose(HANDLE handle){HeapFree(GetProcessHeap(),0,handle);closed++;return NO_ERROR;}
void entry(void){
 NETRESOURCEW resource,*wide;NETRESOURCEA ansi,*a;BYTE buffer[1024];DWORD bytes,error;WCHAR *system;char *tail;unsigned i;
 report=CreateFileA("C:\\MPRTEST.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);if(report==INVALID_HANDLE_VALUE)ExitProcess(2);
 ZeroMemory(&resource,sizeof(resource));resource.lpRemoteName=L"\\\\SERVER\\share\\folder\\file";
 bytes=0;system=(WCHAR*)1;error=WNetGetResourceInformationW(&resource,NULL,&bytes,&system);
 check(error==ERROR_MORE_DATA&&bytes>sizeof(resource)&&system==NULL,"size negotiation");
 for(i=0;i<sizeof(buffer);i++)buffer[i]=0xa5;bytes=1;
 check(WNetGetResourceInformationW(&resource,buffer,&bytes,&system)==ERROR_MORE_DATA&&buffer[0]==0xa5,"small buffer untouched");
 bytes=sizeof(buffer);check(!WNetGetResourceInformationW(&resource,buffer,&bytes,&system),"resource information");wide=(NETRESOURCEW*)buffer;
 check(!lstrcmpW(wide->lpRemoteName,L"\\\\server\\share")&&!lstrcmpW(system,L"\\folder\\file"),"resource and provider-relative suffix");
 check((BYTE*)wide->lpProvider>=buffer&&(BYTE*)system<buffer+bytes,"pointers belong to result buffer");
 resource.lpRemoteName=L"\\\\server\\share\\";bytes=sizeof(buffer);
 check(!WNetGetResourceParentW(&resource,buffer,&bytes),"resource parent");wide=(NETRESOURCEW*)buffer;
 check(!lstrcmpW(wide->lpRemoteName,L"\\\\server")&&wide->dwDisplayType==RESOURCEDISPLAYTYPE_SERVER,"share parent is enumerated server");
 bytes=sizeof(buffer);check(!WNetGetResourceParentW(wide,buffer,&bytes)&&!lstrcmpW(((NETRESOURCEW*)buffer)->lpRemoteName,L"Domain"),"overlapping source and destination");
 ZeroMemory(&ansi,sizeof(ansi));ansi.lpRemoteName="\\\\server\\share\\child";bytes=sizeof(buffer);
 check(!WNetGetResourceInformationA(&ansi,buffer,&bytes,&tail),"ANSI resource query");a=(NETRESOURCEA*)buffer;
 check(!lstrcmpA(a->lpProvider,"Test Provider")&&!lstrcmpA(tail,"\\child"),"ANSI text and suffix");
 resource.lpRemoteName=L"Provider";bytes=sizeof(buffer);check(!WNetGetResourceParentW(&resource,buffer,&bytes)&&((NETRESOURCEW*)buffer)->dwDisplayType==RESOURCEDISPLAYTYPE_ROOT,"provider parent is global root");
 resource.lpRemoteName=L"\\\\server\\missing";bytes=sizeof(buffer);check(WNetGetResourceInformationW(&resource,buffer,&bytes,&system)==ERROR_BAD_NET_NAME,"unknown share rejected");
 resource.lpRemoteName=L"\\\\server\\share";resource.lpProvider=L"Other Provider";bytes=sizeof(buffer);check(WNetGetResourceParentW(&resource,buffer,&bytes)==ERROR_BAD_NET_NAME,"provider filter");resource.lpProvider=NULL;
 check(WNetGetResourceParentW(NULL,buffer,&bytes)==ERROR_INVALID_PARAMETER,"invalid argument");
 networkError=ERROR_NO_NETWORK;check(WNetGetResourceInformationW(&resource,buffer,&bytes,&system)==ERROR_NO_NETWORK,"network error retained");
 check(opened==closed&&growth>0,"enumerators closed and buffer growth exercised");check(TRUE,"ALL");CloseHandle(report);ExitProcess(0);
}
