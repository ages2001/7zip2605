/* Windows 3.x File Manager associations. Preserve previous/user-owned values.
   The profile APIs are available in Windows 3.0, including real mode.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_ASSOCIATIONS_H
#define Z7_WIN16_ASSOCIATIONS_H
#define ASSOC_SECTION "7-Zip x86-16"
static int assocRead(const char *section,const char *key,char *value,int *present){
 unsigned n=GetProfileString(section,key,"!",value,260);
 if(n>=259)return 0;*present=1;
 /* Windows 3.x trims control characters in default strings. Two printable
    defaults distinguish absence even if a real value equals the sentinel. */
 if(!strcmp(value,"!")){char probe[4];GetProfileString(section,key,"?",probe,sizeof(probe));*present=strcmp(probe,"?")!=0;}
 if(!*present)value[0]=0;return 1;
}
static int assocCommand(const char *exe,const char *ext,char *command){
 if(strlen(exe)>240||strchr(exe,'"')||strchr(exe,'\r')||strchr(exe,'\n'))return 0;
 sprintf(command,"%s ^.%s",exe,ext);return 1;
}
static int assocCurrent(const char *exe,const char *ext){
 char command[260],current[260];int present;
 return assocCommand(exe,ext,command)&&assocRead("Extensions",ext,current,&present)&&present&&!stricmp(current,command);
}
static int assocClear(const char *ext){
 char key[24];int ok=1;
 sprintf(key,"%s.Previous",ext);if(!WriteProfileString(ASSOC_SECTION,key,NULL))ok=0;
 sprintf(key,"%s.Present",ext);if(!WriteProfileString(ASSOC_SECTION,key,NULL))ok=0;
 /* Delete ownership last. A failed cleanup can be retried safely. */
 if(ok){sprintf(key,"%s.Owner",ext);if(!WriteProfileString(ASSOC_SECTION,key,NULL))ok=0;}
 return ok;
}
static int assocSet(const char *exe,const char *ext,int enable){
 char command[260],current[260],owner[260],previous[260],key[24],flag[260];int hasCurrent,hasOwner,hasPrevious,hasFlag;
 if(!assocCommand(exe,ext,command)||!assocRead("Extensions",ext,current,&hasCurrent))return 0;
 sprintf(key,"%s.Owner",ext);if(!assocRead(ASSOC_SECTION,key,owner,&hasOwner))return 0;
 if(hasOwner&&stricmp(owner,command))return !enable; /* Another install owns the restoration record. */
 if(!enable){
  if(!hasOwner)return !hasCurrent||stricmp(current,command)||WriteProfileString("Extensions",ext,NULL);
  if(hasCurrent&&!stricmp(current,command)){
   sprintf(key,"%s.Present",ext);if(!assocRead(ASSOC_SECTION,key,flag,&hasFlag)||!hasFlag||(strcmp(flag,"0")&&strcmp(flag,"1")))return 0;
   sprintf(key,"%s.Previous",ext);if(!assocRead(ASSOC_SECTION,key,previous,&hasPrevious)||(!strcmp(flag,"1")&&!hasPrevious))return 0;
   if(!WriteProfileString("Extensions",ext,!strcmp(flag,"1")?previous:NULL))return 0;
  }
  /* If a user changed the association afterwards, retain their new value. */
  return assocClear(ext);
 }
 if(hasOwner&&hasCurrent&&!stricmp(current,command))return 1;
 if(hasOwner){
  /* A subsequent explicit Apply can replace a user change, preserving that
     current value as the new restoration target. */
  if(!assocClear(ext))return 0;
 }
 if(hasCurrent&&!stricmp(current,command))return 1; /* No owned record: do not invent its history. */
 sprintf(key,"%s.Previous",ext);if(!WriteProfileString(ASSOC_SECTION,key,hasCurrent?current:NULL))return 0;
 sprintf(key,"%s.Present",ext);if(!WriteProfileString(ASSOC_SECTION,key,hasCurrent?"1":"0")){assocClear(ext);return 0;}
 sprintf(key,"%s.Owner",ext);if(!WriteProfileString(ASSOC_SECTION,key,command)){assocClear(ext);return 0;}
 if(!WriteProfileString("Extensions",ext,command)){
  /* Keep the record if restoration fails, so a later retry can recover it. */
  if(WriteProfileString("Extensions",ext,hasCurrent?current:NULL))assocClear(ext);return 0;
 }
 return assocCurrent(exe,ext);
}
static int assocRemoveAll(const char *exe){int a=assocSet(exe,"7z",0),b=assocSet(exe,"zip",0);return a&&b;}
#endif
