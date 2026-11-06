/* Profile API failure/ownership tests. No real host associations are changed. */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <assert.h>
#define stricmp strcasecmp
typedef struct {char section[40],key[40],value[600];int used;} Setting;
static Setting settings[16];static unsigned writes,failAt;
static int lookup(const char *section,const char *key){unsigned i;for(i=0;i<16;i++)if(settings[i].used&&!strcasecmp(section,settings[i].section)&&!strcasecmp(key,settings[i].key))return (int)i;return -1;}
static unsigned GetProfileString(const char *section,const char *key,const char *fallback,char *out,unsigned cap){int i=lookup(section,key);const char *value=i<0?fallback:settings[i].value;size_t n=strlen(value);if(n>=cap)n=cap-1;memcpy(out,value,n);out[n]=0;return (unsigned)n;}
static int WriteProfileString(const char *section,const char *key,const char *value){int i;if(++writes==failAt)return 0;i=lookup(section,key);if(!value){if(i>=0)settings[i].used=0;return 1;}if(i<0){for(i=0;i<16&&settings[i].used;i++);if(i==16)return 0;}settings[i].used=1;strcpy(settings[i].section,section);strcpy(settings[i].key,key);strcpy(settings[i].value,value);return 1;}
#include "associations.h"
static const char *exe="C:\\7-Zip\\7ZFM16.EXE";
static void reset(void){memset(settings,0,sizeof(settings));writes=failAt=0;}
static void expect(const char *ext,const char *value){int i=lookup("Extensions",ext);assert(value?i>=0&&!strcmp(settings[i].value,value):i<0);}
int main(void){unsigned fail,count;char longValue[300];
 reset();WriteProfileString("Extensions","7z","OLD.EXE ^.7z");assert(assocSet(exe,"7z",1));assert(assocCurrent(exe,"7z"));assert(assocSet(exe,"zip",1));assert(assocCurrent(exe,"zip"));assert(assocSet(exe,"7z",1));assert(assocRemoveAll(exe));expect("7z","OLD.EXE ^.7z");expect("zip",NULL);
 reset();assert(assocSet(exe,"7z",1));WriteProfileString("Extensions","7z","NEW.EXE ^.7z");assert(assocRemoveAll(exe));expect("7z","NEW.EXE ^.7z");
 reset();assert(assocSet(exe,"zip",1));assert(!assocSet("D:\\OTHER\\7ZFM16.EXE","zip",1));assert(assocRemoveAll("D:\\OTHER\\7ZFM16.EXE"));assert(assocCurrent(exe,"zip"));assert(assocRemoveAll(exe));expect("zip",NULL);
 reset();assert(assocSet(exe,"zip",1));WriteProfileString("Extensions","zip","USER.EXE ^.zip");assert(assocSet(exe,"zip",1));assert(assocRemoveAll(exe));expect("zip","USER.EXE ^.zip");
 for(fail=1;fail<=4;fail++){
  reset();WriteProfileString("Extensions","7z","OLD.EXE ^.7z");writes=0;failAt=fail;assert(!assocSet(exe,"7z",1));expect("7z","OLD.EXE ^.7z");failAt=0;assert(assocSet(exe,"7z",1));assert(assocRemoveAll(exe));expect("7z","OLD.EXE ^.7z");
 }
 for(fail=1;fail<=4;fail++){
  reset();WriteProfileString("Extensions","7z","OLD.EXE ^.7z");assert(assocSet(exe,"7z",1));writes=0;failAt=fail;assert(!assocSet(exe,"7z",0));failAt=0;assert(assocSet(exe,"7z",0));expect("7z","OLD.EXE ^.7z");
 }
 reset();WriteProfileString("Extensions","zip","!");assert(assocSet(exe,"zip",1));assert(assocRemoveAll(exe));expect("zip","!");
 reset();WriteProfileString("Extensions","zip","C:\\7-Zip\\7ZFM16.EXE ^.zip");assert(assocCurrent(exe,"zip"));assert(assocRemoveAll(exe));expect("zip",NULL);
 reset();memset(longValue,'x',299);longValue[299]=0;WriteProfileString("Extensions","zip",longValue);assert(!assocSet(exe,"zip",1));expect("zip",longValue);
 reset();assert(assocSet(exe,"7z",1));WriteProfileString(ASSOC_SECTION,"7z.Present","invalid");assert(!assocRemoveAll(exe));assert(assocCurrent(exe,"7z"));
 count=0;for(fail=0;fail<16;fail++)count+=settings[fail].used;assert(count>0);
 puts("PASS association enable/restore, absent values, later user changes, competing installs, truncated/corrupt records, all enable/restore write failures and retry");return 0;
}
