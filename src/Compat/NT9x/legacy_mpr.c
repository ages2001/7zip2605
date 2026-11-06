/* Resource queries for NT 3.1, using its network-provider enumeration APIs.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnetwk.h>
#define BIND(name, bytes) __typeof__(&name) legacy_import_##name __asm__("__imp__" #name "@" #bytes) = name
#ifdef NT9X_MPR_TEST
extern DWORD WINAPI TestNetOpen(DWORD,DWORD,DWORD,LPNETRESOURCEW,LPHANDLE);
extern DWORD WINAPI TestNetEnum(HANDLE,LPDWORD,LPVOID,LPDWORD);
extern DWORD WINAPI TestNetClose(HANDLE);
#define OPEN TestNetOpen
#define ENUM TestNetEnum
#define CLOSE TestNetClose
#define NATIVE(name) NULL
#else
#define OPEN WNetOpenEnumW
#define ENUM WNetEnumResourceW
#define CLOSE WNetCloseEnum
static FARPROC native(const char *name){HMODULE m=GetModuleHandleA("mpr.dll");return m?GetProcAddress(m,name):NULL;}
#define NATIVE(name) native(#name)
#endif
static void *allocate(SIZE_T size){return HeapAlloc(GetProcessHeap(),HEAP_ZERO_MEMORY,size);}
static void release(void *p){if(p)HeapFree(GetProcessHeap(),0,p);}
static WCHAR *duplicate(const WCHAR *s){WCHAR *p;SIZE_T n;if(!s)return NULL;n=(lstrlenW(s)+1)*sizeof(WCHAR);p=allocate(n);if(p)CopyMemory(p,s,n);return p;}
static DWORD stringBytes(const WCHAR *s,BOOL wide){int n;if(!s)return 0;if(wide)return (lstrlenW(s)+1)*sizeof(WCHAR);n=WideCharToMultiByte(CP_ACP,0,s,-1,NULL,0,NULL,NULL);return n>0?(DWORD)n:0;}
/* Calculate the entire buffer before writing anything: source and destination
   are allowed to overlap in the public API. The search result is a private copy. */
static DWORD pack(const NETRESOURCEW *r,const WCHAR *system,void *buffer,DWORD *size,void **systemOut,BOOL wide){
 const WCHAR *strings[5];DWORD lengths[5],needed=sizeof(NETRESOURCEW);unsigned i;BYTE *next;NETRESOURCEW *out;void **fields[5];
 strings[0]=r->lpLocalName;strings[1]=r->lpRemoteName;strings[2]=r->lpComment;strings[3]=r->lpProvider;strings[4]=system;
 for(i=0;i<5;i++){lengths[i]=stringBytes(strings[i],wide);if(strings[i]&&!lengths[i])return ERROR_NO_UNICODE_TRANSLATION;if(lengths[i]>MAXDWORD-needed)return ERROR_NOT_ENOUGH_MEMORY;needed+=lengths[i];}
 if(!buffer||*size<needed){*size=needed;return ERROR_MORE_DATA;}
 out=buffer;*out=*r;next=(BYTE*)buffer+sizeof(*out);
 fields[0]=(void**)&out->lpLocalName;fields[1]=(void**)&out->lpRemoteName;fields[2]=(void**)&out->lpComment;fields[3]=(void**)&out->lpProvider;fields[4]=systemOut;
 for(i=0;i<5;i++){
  if(fields[i])*fields[i]=strings[i]?next:NULL;
  if(lengths[i]){if(wide)CopyMemory(next,strings[i],lengths[i]);else{BOOL substituted=FALSE;if(!WideCharToMultiByte(CP_ACP,0,strings[i],-1,(char*)next,lengths[i],NULL,&substituted)||substituted)return ERROR_NO_UNICODE_TRANSLATION;}next+=lengths[i];}
 }
 *size=needed;return NO_ERROR;
}
static NETRESOURCEW *clone(const NETRESOURCEW *r){DWORD size=0;NETRESOURCEW *out;pack(r,NULL,NULL,&size,NULL,TRUE);out=allocate(size);if(out)pack(r,NULL,out,&size,NULL,TRUE);return out;}
typedef struct Search {
 const WCHAR *name,*provider;BOOL parent;NETRESOURCEW *found,*ancestor;
 unsigned visited;DWORD error;
 const NETRESOURCEW *stack[32];
} Search;
static BOOL same(const WCHAR *a,const WCHAR *b){return (!a&&!b)||(a&&b&&!lstrcmpiW(a,b));}
static BOOL prefix(const WCHAR *name,const WCHAR *resource){
 unsigned n,i;if(!resource)return FALSE;n=lstrlenW(resource);if(!n||lstrlenW(name)<(int)n)return FALSE;
 for(i=0;i<n;i++){WCHAR a[2]={name[i],0},b[2]={resource[i],0};if(lstrcmpiW(a,b))return FALSE;}
 return !name[n]||name[n]=='\\';
}
static DWORD remember(Search *s,const NETRESOURCEW *r,const NETRESOURCEW *parent){
 NETRESOURCEW root;NETRESOURCEW *a,*b;
 ZeroMemory(&root,sizeof(root));root.dwScope=RESOURCE_GLOBALNET;root.dwType=RESOURCETYPE_ANY;root.dwDisplayType=RESOURCEDISPLAYTYPE_ROOT;root.dwUsage=RESOURCEUSAGE_CONTAINER;
 a=clone(r);b=clone(parent?parent:&root);if(!a||!b){release(a);release(b);return ERROR_NOT_ENOUGH_MEMORY;}
 release(s->found);release(s->ancestor);s->found=a;s->ancestor=b;return NO_ERROR;
}
/* Enumerate only matching UNC containers. Domain/provider containers do not
   share a UNC prefix and must be visited to find their server children. */
static DWORD walk(Search *s,const NETRESOURCEW *parent,unsigned depth){
 HANDLE enumeration;DWORD error,size=16384,count,i;BYTE *buffer;
 if(depth>=32)return ERROR_NOT_SUPPORTED;
 for(i=0;i+1<depth;i++)if(parent&&same(s->stack[i]->lpRemoteName,parent->lpRemoteName)&&same(s->stack[i]->lpProvider,parent->lpProvider))return NO_ERROR;
 error=OPEN(RESOURCE_GLOBALNET,RESOURCETYPE_ANY,0,(LPNETRESOURCEW)parent,&enumeration);
 if(error){if(!s->error)s->error=error;return NO_ERROR;}
 buffer=allocate(size);if(!buffer){CLOSE(enumeration);return ERROR_NOT_ENOUGH_MEMORY;}
 if(parent)s->stack[depth-1]=parent;
 for(;;){
  DWORD bytes=size;count=1;error=ENUM(enumeration,&count,buffer,&bytes);
  if(error==ERROR_MORE_DATA){BYTE *larger;if(bytes<=size){if(size>=16*1024*1024){error=ERROR_NOT_ENOUGH_MEMORY;break;}bytes=size*2;}if(bytes>16*1024*1024){error=ERROR_NOT_ENOUGH_MEMORY;break;}larger=allocate(bytes);if(!larger){error=ERROR_NOT_ENOUGH_MEMORY;break;}release(buffer);buffer=larger;size=bytes;continue;}
  if(error==ERROR_NO_MORE_ITEMS){error=NO_ERROR;break;}
  if(error){if(!s->error)s->error=error;error=NO_ERROR;break;}
  if(!count){error=NO_ERROR;break;}
  if(count!=1){error=ERROR_INVALID_DATA;break;}
  if(++s->visited>65536){error=ERROR_NOT_SUPPORTED;break;}
  {NETRESOURCEW *r=(NETRESOURCEW*)buffer;BOOL match;
   if(s->provider&&r->lpProvider&&!same(s->provider,r->lpProvider))continue;
   match=prefix(s->name,r->lpRemoteName);
   if(match&&(!s->found||lstrlenW(r->lpRemoteName)>lstrlenW(s->found->lpRemoteName))){error=remember(s,r,parent);if(error)break;}
   if(match&&!lstrcmpiW(s->name,r->lpRemoteName))break;
   if((r->dwUsage&RESOURCEUSAGE_CONTAINER)&&(!r->lpRemoteName||r->lpRemoteName[0]!='\\'||match)){
    error=walk(s,r,depth+1);if(error)break;
    if(s->found&&!lstrcmpiW(s->name,s->found->lpRemoteName))break;
   }
  }
 }
 release(buffer);CLOSE(enumeration);return error;
}
static WCHAR *toWide(const char *s){int n;WCHAR *p;if(!s)return NULL;n=MultiByteToWideChar(CP_ACP,0,s,-1,NULL,0);if(n<=0)return NULL;p=allocate(n*sizeof(WCHAR));if(p)MultiByteToWideChar(CP_ACP,0,s,-1,p,n);return p;}
static DWORD query(const void *input,void *buffer,DWORD *size,void **system,BOOL parent,BOOL wide){
 const NETRESOURCEW *r=input;Search search;WCHAR *name,*provider;DWORD error;unsigned n;
 if(!input||!size||(!parent&&!system))return ERROR_INVALID_PARAMETER;
 if(system)*system=NULL;
 if(r->dwType!=RESOURCETYPE_ANY&&r->dwType!=RESOURCETYPE_DISK&&r->dwType!=RESOURCETYPE_PRINT)return ERROR_INVALID_PARAMETER;
 if(!r->lpRemoteName)return ERROR_BAD_NET_NAME;
 name=wide?duplicate(r->lpRemoteName):toWide((const char*)r->lpRemoteName);
 provider=wide?duplicate(r->lpProvider):toWide((const char*)r->lpProvider);
 if(!name||(r->lpProvider&&!provider)){release(name);release(provider);return ERROR_NOT_ENOUGH_MEMORY;}
 n=lstrlenW(name);while(n>2&&name[n-1]=='\\')name[--n]=0;
 if(!n){release(name);release(provider);return ERROR_BAD_NET_NAME;}
 ZeroMemory(&search,sizeof(search));search.name=name;search.provider=provider;search.parent=parent;
 error=walk(&search,NULL,0);
 if(!error){
  if(!search.found)error=search.error?search.error:ERROR_BAD_NET_NAME;
  else if((parent||!(search.found->dwUsage&RESOURCEUSAGE_CONNECTABLE))&&lstrcmpiW(name,search.found->lpRemoteName))error=ERROR_BAD_NET_NAME;
  else{const WCHAR *rest=name+lstrlenW(search.found->lpRemoteName);error=pack(parent?search.ancestor:search.found,parent||!*rest?NULL:rest,buffer,size,system,wide);}
 }
 release(search.found);release(search.ancestor);release(provider);release(name);return error;
}
DWORD WINAPI WNetGetResourceInformationW(LPNETRESOURCEW r,LPVOID b,LPDWORD n,LPWSTR *s){__typeof__(&WNetGetResourceInformationW) fn=(__typeof__(&WNetGetResourceInformationW))NATIVE(WNetGetResourceInformationW);return fn?fn(r,b,n,s):query(r,b,n,(void**)s,FALSE,TRUE);}
BIND(WNetGetResourceInformationW,16);
DWORD WINAPI WNetGetResourceInformationA(LPNETRESOURCEA r,LPVOID b,LPDWORD n,LPSTR *s){__typeof__(&WNetGetResourceInformationA) fn=(__typeof__(&WNetGetResourceInformationA))NATIVE(WNetGetResourceInformationA);return fn?fn(r,b,n,s):query(r,b,n,(void**)s,FALSE,FALSE);}
BIND(WNetGetResourceInformationA,16);
DWORD WINAPI WNetGetResourceParentW(LPNETRESOURCEW r,LPVOID b,LPDWORD n){__typeof__(&WNetGetResourceParentW) fn=(__typeof__(&WNetGetResourceParentW))NATIVE(WNetGetResourceParentW);return fn?fn(r,b,n):query(r,b,n,NULL,TRUE,TRUE);}
BIND(WNetGetResourceParentW,12);
DWORD WINAPI WNetGetResourceParentA(LPNETRESOURCEA r,LPVOID b,LPDWORD n){__typeof__(&WNetGetResourceParentA) fn=(__typeof__(&WNetGetResourceParentA))NATIVE(WNetGetResourceParentA);return fn?fn(r,b,n):query(r,b,n,NULL,TRUE,FALSE);}
BIND(WNetGetResourceParentA,12);
