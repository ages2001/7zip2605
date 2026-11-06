/* Size the remembered Add/Update input to its actual length in Win16 memory.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_CREATE_SOURCE_H
#define Z7_WIN16_CREATE_SOURCE_H
#define Z7_CREATE_SOURCE_LIMIT 4095
static HGLOBAL createSourceHandle;
static char *createSource;
static void freeCreateSource(void){
 if(createSourceHandle){GlobalUnlock(createSourceHandle);GlobalFree(createSourceHandle);}
 createSourceHandle=0;createSource=NULL;
}
static int replaceCreateSource(const char *text,HWND edit){
 HGLOBAL handle;char *copy;unsigned length;
 if(edit){int count=GetWindowTextLength(edit);if(count<0||count>Z7_CREATE_SOURCE_LIMIT)return 0;length=(unsigned)count;}
 else{size_t count=strlen(text);if(count>Z7_CREATE_SOURCE_LIMIT)return 0;length=(unsigned)count;}
 handle=GlobalAlloc(GMEM_MOVEABLE,(DWORD)length+1);if(!handle)return 0;
 copy=(char *)GlobalLock(handle);if(!copy){GlobalFree(handle);return 0;}
 if(edit){if(GetWindowText(edit,copy,length+1)!=(int)length){GlobalUnlock(handle);GlobalFree(handle);return 0;}}
 else memcpy(copy,text,length+1);
 freeCreateSource();createSourceHandle=handle;createSource=copy;return 1;
}
static int setCreateSource(const char *text){return replaceCreateSource(text,0);}
#endif
