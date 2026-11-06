/* Typed registry values for Win32s.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later

   Native Win32s 1.30 stores registry data in the Windows 3.1 REG.DAT database.
   Observed behavior (experiments/win32s91/registry-native.txt):
   - only REG_SZ is accepted; REG_DWORD/REG_BINARY fail with error 87;
   - a named value is the default value of a same-named subkey;
   - RegDeleteValueA and RegEnumValueA fail with error 120;
   - RegDeleteKeyA deletes a whole subtree;
   - RegEnumKeyA reports the end of a list as ERROR_FILE_NOT_FOUND;
   - a missing value is reported as ERROR_BADKEY (1010), not 2;
   - a too-small buffer is silently truncated and still reports success.

   On Win32s only, this adapter encodes non-string data in an unambiguous
   REG_SZ form, returns standard Win32 status codes and sizes, and deletes a
   named value by deleting its value-only subkey. NT and Windows 9x always use
   the native functions unchanged. Data that cannot be represented is never
   silently truncated: the call fails and the caller keeps its defaults. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define BIND(name,bytes) __typeof__(&name) w32s_reg_import_##name __asm__("__imp__" #name "@" #bytes)=w32s_##name
#define NATIVE(name) ((__typeof__(&name))GetProcAddress(GetModuleHandleA("advapi32.dll"),#name))

static const char kPrefix[] = "~7zRV1~";   /* followed by 8 hex type digits, ':' and base64 */
static const char kDeleted[] = "~7zRV1~DELETED";
#define PREFIX_LEN 7
/* Win32s REG.DAT strings were verified up to 8192 bytes including the NUL. */
#define MAX_NATIVE_TEXT 8192

static BOOL isWin32s(void){DWORD v=GetVersion();return (v&0x80000000UL)&&(v&255)==3;}
static void *alloc(DWORD n){return HeapAlloc(GetProcessHeap(),0,n?n:1);}
static void release(void *p){if(p)HeapFree(GetProcessHeap(),0,p);}
static BOOL startsWith(const char *s,DWORD n,const char *prefix){DWORD i;for(i=0;prefix[i];i++)if(i>=n||s[i]!=prefix[i])return FALSE;return TRUE;}

static const char kAlphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static int decode64(char c){
  if(c>='A'&&c<='Z')return c-'A';
  if(c>='a'&&c<='z')return c-'a'+26;
  if(c>='0'&&c<='9')return c-'0'+52;
  if(c=='+')return 62;
  if(c=='/')return 63;
  return -1;
}
static int hexDigit(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;return -1;}

/* Returns a heap string or NULL (last error set). */
static char *encode(DWORD type,const BYTE *data,DWORD bytes){
  DWORD groups=(bytes+2)/3,length,i,o;char *text;
  if(bytes>0x3fffffffUL){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
  length=PREFIX_LEN+9+groups*4;
  if(length+1>MAX_NATIVE_TEXT){SetLastError(ERROR_INVALID_PARAMETER);return NULL;}
  text=alloc(length+1);if(!text){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return NULL;}
  CopyMemory(text,kPrefix,PREFIX_LEN);
  for(i=0;i<8;i++)text[PREFIX_LEN+i]="0123456789ABCDEF"[(type>>(28-4*i))&15];
  text[PREFIX_LEN+8]=':';o=PREFIX_LEN+9;
  for(i=0;i<bytes;i+=3){
    DWORD v=(DWORD)data[i]<<16;
    if(i+1<bytes)v|=(DWORD)data[i+1]<<8;
    if(i+2<bytes)v|=data[i+2];
    text[o++]=kAlphabet[(v>>18)&63];text[o++]=kAlphabet[(v>>12)&63];
    text[o++]=i+1<bytes?kAlphabet[(v>>6)&63]:'=';text[o++]=i+2<bytes?kAlphabet[v&63]:'=';
  }
  text[o]=0;return text;
}

/* Decodes text[0..n) (no terminator counted). Returns FALSE if not an encoded value. */
static BOOL decode(const char *text,DWORD n,DWORD *type,BYTE **out,DWORD *bytes){
  DWORD i,t=0,body,o=0;BYTE *data;
  if(n<PREFIX_LEN+9||!startsWith(text,n,kPrefix)||text[PREFIX_LEN+8]!=':')return FALSE;
  for(i=0;i<8;i++){int d=hexDigit(text[PREFIX_LEN+i]);if(d<0)return FALSE;t=(t<<4)|(DWORD)d;}
  body=n-PREFIX_LEN-9;if(body%4)return FALSE;
  data=alloc(body/4*3);if(!data)return FALSE;
  for(i=0;i<body;i+=4){
    const char *q=text+PREFIX_LEN+9+i;int a=decode64(q[0]),b=decode64(q[1]),c=q[2]=='='?0:decode64(q[2]),d=q[3]=='='?0:decode64(q[3]);
    DWORD v;
    if(a<0||b<0||c<0||d<0||(q[2]=='='&&q[3]!='=')||((q[2]=='='||q[3]=='=')&&i+4!=body)){release(data);return FALSE;}
    v=((DWORD)a<<18)|((DWORD)b<<12)|((DWORD)c<<6)|(DWORD)d;
    data[o++]=(BYTE)(v>>16);if(q[2]!='=')data[o++]=(BYTE)(v>>8);if(q[3]!='=')data[o++]=(BYTE)v;
  }
  *type=t;*out=data;*bytes=o;return TRUE;
}

static BOOL plainString(DWORD type,const BYTE *data,DWORD bytes){
  DWORD i;
  if(type!=REG_SZ||!data||!bytes||data[bytes-1])return FALSE;
  for(i=0;i+1<bytes;i++)if(!data[i])return FALSE;
  return !startsWith((const char*)data,bytes-1,kPrefix);
}

static LONG WINAPI w32s_RegSetValueExA(HKEY key,LPCSTR name,DWORD reserved,DWORD type,const BYTE *data,DWORD bytes){
  __typeof__(&RegSetValueExA) native=NATIVE(RegSetValueExA);char *text;LONG r;
  if(!isWin32s())return native(key,name,reserved,type,data,bytes);
  if(reserved||(!data&&bytes))return ERROR_INVALID_PARAMETER;
  if(plainString(type,data,bytes)){
    if(bytes>MAX_NATIVE_TEXT)return ERROR_INVALID_PARAMETER;
    return native(key,name,0,REG_SZ,data,bytes);
  }
  text=encode(type,data,bytes);if(!text)return (LONG)GetLastError();
  r=native(key,name,0,REG_SZ,(const BYTE*)text,lstrlenA(text)+1);release(text);return r;
}
BIND(RegSetValueExA,24);

/* Reads the complete native string. Returns a Win32 status. */
static LONG readNative(HKEY key,LPCSTR name,char **text,DWORD *size){
  __typeof__(&RegQueryValueExA) native=NATIVE(RegQueryValueExA);DWORD type=0,n=0,m;char *buffer;LONG r;
  *text=NULL;*size=0;
  r=native(key,name,NULL,&type,NULL,&n);
  if(r==ERROR_BADKEY||r==ERROR_FILE_NOT_FOUND||r==ERROR_PATH_NOT_FOUND)return ERROR_FILE_NOT_FOUND;
  if(r)return r;
  if(n>MAX_NATIVE_TEXT*2)return ERROR_NOT_ENOUGH_MEMORY;
  buffer=alloc(n+1);if(!buffer)return ERROR_NOT_ENOUGH_MEMORY;
  m=n+1;r=native(key,name,NULL,&type,(BYTE*)buffer,&m);
  if(r==ERROR_BADKEY)r=ERROR_FILE_NOT_FOUND;
  if(r){release(buffer);return r;}
  if(type!=REG_SZ){release(buffer);return ERROR_INVALID_DATA;}
  if(m>n+1)m=n+1;
  buffer[m]=0;
  *text=buffer;*size=m;return ERROR_SUCCESS;
}

static LONG WINAPI w32s_RegQueryValueExA(HKEY key,LPCSTR name,LPDWORD reserved,LPDWORD type,LPBYTE data,LPDWORD bytes){
  __typeof__(&RegQueryValueExA) native=NATIVE(RegQueryValueExA);
  char *text;DWORD size,length,t=REG_SZ,required,capacity;BYTE *decoded=NULL;const BYTE *source;LONG r;
  if(!isWin32s())return native(key,name,reserved,type,data,bytes);
  if(reserved||(data&&!bytes))return ERROR_INVALID_PARAMETER;
  capacity=bytes?*bytes:0;
  r=readNative(key,name,&text,&size);if(r)return r;
  length=lstrlenA(text);
  if(length==sizeof(kDeleted)-1&&startsWith(text,length,kDeleted)){release(text);return ERROR_FILE_NOT_FOUND;}
  if(decode(text,length,&t,&decoded,&required))source=decoded;
  else{t=REG_SZ;source=(const BYTE*)text;required=length+1;}
  if(type)*type=t;
  if(bytes)*bytes=required;
  r=ERROR_SUCCESS;
  if(data){if(capacity<required)r=ERROR_MORE_DATA;else if(required)CopyMemory(data,source,required);}
  release(decoded);release(text);return r;
}
BIND(RegQueryValueExA,24);

static LONG WINAPI w32s_RegDeleteValueA(HKEY key,LPCSTR name){
  __typeof__(&RegDeleteValueA) native=NATIVE(RegDeleteValueA);
  __typeof__(&RegSetValueExA) set=NATIVE(RegSetValueExA);
  __typeof__(&RegOpenKeyExA) open=NATIVE(RegOpenKeyExA);
  __typeof__(&RegEnumKeyA) enumKey=NATIVE(RegEnumKeyA);
  __typeof__(&RegDeleteKeyA) deleteKey=NATIVE(RegDeleteKeyA);
  __typeof__(&RegCloseKey) closeKey=NATIVE(RegCloseKey);
  char *text,child[MAX_PATH+1];DWORD size;HKEY sub;LONG r,children;
  if(!isWin32s())return native(key,name);
  r=readNative(key,name,&text,&size);if(r)return r;
  size=lstrlenA(text);
  if(size==sizeof(kDeleted)-1&&startsWith(text,size,kDeleted)){release(text);return ERROR_FILE_NOT_FOUND;}
  release(text);
  /* The default value of a key cannot be removed in REG.DAT. A named value
     whose subkey also has children must not delete those children. */
  if(name&&*name){
    r=open(key,name,0,KEY_READ,&sub);if(r)return r;
    children=enumKey(sub,0,child,sizeof(child));closeKey(sub);
    /* Win32s reports the end of a key list as ERROR_FILE_NOT_FOUND. */
    if(children==ERROR_NO_MORE_ITEMS||children==ERROR_FILE_NOT_FOUND)return deleteKey(key,name);
    if(children!=ERROR_SUCCESS)return children;
  }
  return set(key,name,0,REG_SZ,(const BYTE*)kDeleted,sizeof(kDeleted));
}
BIND(RegDeleteValueA,8);
