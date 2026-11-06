/* Shared native DOS path validation. SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_DOS_PATHS_H
#define Z7_WIN16_DOS_PATHS_H
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef __WATCOMC__
#include <direct.h>
#define Z7_PATH_SEPARATOR '\\'
#define Z7_MKDIR(path) mkdir(path)
#else
#include <unistd.h>
#define Z7_PATH_SEPARATOR '/'
#define Z7_MKDIR(path) mkdir(path,0700)
#endif
static unsigned foldName(unsigned ch){return ch>='a'&&ch<='z'?ch-'a'+'A':ch;}
static int sameName(const char *a,const char *b){
 while(*a&&*b&&foldName((unsigned char)*a)==foldName((unsigned char)*b)){a++;b++;}
 return !*a&&!*b;
}
static int namePrefix(const char *a,const char *b){
 while(*a&&*b&&foldName((unsigned char)*a)==foldName((unsigned char)*b)){a++;b++;}
 return !*a&&*b==Z7_PATH_SEPARATOR;
}
static int validDosComponent(const char *name,unsigned length){
 unsigned i,base=0,extension=0,dot=0;char device[9];
 if(!length)return 0;
 for(i=0;i<length;i++){
  unsigned ch=(unsigned char)name[i];
  if(ch=='.'){if(dot||!base)return 0;dot=1;continue;}
  if(!((ch>='A'&&ch<='Z')||(ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')
       ||strchr("_$~!#%&'()-@^{}`",ch))||ch==' '||ch>=127||ch<33)return 0;
  if(dot){if(++extension>3)return 0;}else{if(base==8)return 0;device[base++]=(char)foldName(ch);}
 }
 if(dot&&!extension)return 0;device[base]=0;
 if(!strcmp(device,"CON")||!strcmp(device,"PRN")||!strcmp(device,"AUX")||!strcmp(device,"NUL")||!strcmp(device,"CLOCK$"))return 0;
 if(base==4&&device[3]>='0'&&device[3]<='9'
     &&((device[0]=='C'&&device[1]=='O'&&device[2]=='M')||(device[0]=='L'&&device[1]=='P'&&device[2]=='T')))return 0;
 return 1;
}
static int validDosDestination(const char *path){
 const char *part=path,*end;
 if(part[0]&&part[1]==':'){
  unsigned drive=foldName((unsigned char)part[0]);
  if(drive<'A'||drive>'Z')return 0;
  part+=2;
 }
 if(*part=='\\'||*part=='/')part++;
 if(!*part)return 0;
 for(;;){
  end=part;while(*end&&*end!='\\'&&*end!='/')end++;
  if(!validDosComponent(part,(unsigned)(end-part))){
   /* Relative parent components are legal only before the new directory. */
   if(!*end||!((end-part==1&&part[0]=='.')
       ||(end-part==2&&part[0]=='.'&&part[1]=='.')))return 0;
  }
  if(!*end)return 1;
  part=end+1;
 }
}
#endif
