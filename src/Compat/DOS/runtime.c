/* DOS platform support; no Windows runtime is required. */
#include "runtime.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <dos.h>
#include <sys/stat.h>
#include <utime.h>
#include <fcntl.h>
#include <ctype.h>
#include "unicode_upper.h"

wchar_t *wmemcpy(wchar_t *d, const wchar_t *s, size_t n)
{ return (wchar_t *)memcpy(d, s, n * sizeof(wchar_t)); }
wchar_t *wmemmove(wchar_t *d, const wchar_t *s, size_t n)
{ return (wchar_t *)memmove(d, s, n * sizeof(wchar_t)); }
int wcscmp(const wchar_t *a, const wchar_t *b)
{ while (*a && *a == *b) { ++a; ++b; } return *a < *b ? -1 : *a != *b; }
wchar_t *wcsstr(const wchar_t *s, const wchar_t *find)
{
  if (!*find) return (wchar_t *)s;
  for (; *s; ++s) {
    const wchar_t *a=s, *b=find;
    while (*b && *a == *b) { ++a; ++b; }
    if (!*b) return (wchar_t *)s;
  }
  return NULL;
}
wint_t towupper(wint_t value)
{
  size_t lo=0, hi=sizeof(dos_upper)/sizeof(dos_upper[0]);
  while (lo<hi) {
    size_t mid=lo+(hi-lo)/2;
    if (dos_upper[mid][0] < (unsigned long)value) lo=mid+1;
    else if (dos_upper[mid][0] > (unsigned long)value) hi=mid;
    else return (wint_t)dos_upper[mid][1];
  }
  return value;
}
static void warnAccessTime(const struct timespec *t)
{
  static int warned;
  if (t && t[0].tv_nsec != UTIME_OMIT && !warned) {
    fputs("WARNING: DOS cannot preserve the requested access timestamp. Original metadata remains in the source archive.\n", stderr);
    warned=1;
  }
}
static int modificationTime(const struct timespec *t, time_t *value)
{
  if (t && t[1].tv_nsec == UTIME_OMIT) return 0;
  if (!t || t[1].tv_nsec == UTIME_NOW) *value=time(NULL);
  else {
    if (t[1].tv_nsec<0 || t[1].tv_nsec>=1000000000L) { errno=EINVAL; return -1; }
    *value=t[1].tv_sec;
  }
  return 1;
}
int futimens(int fd, const struct timespec t[2])
{
  time_t value; struct tm *local; struct ftime ft;
  int changed=modificationTime(t,&value);
  if(changed<0)return -1;
  warnAccessTime(t);
  if(!changed)return 0;
  local=localtime(&value);
  if(!local || local->tm_year<80 || local->tm_year>207) { errno=ERANGE; return -1; }
  memset(&ft,0,sizeof(ft));
  ft.ft_year=local->tm_year-80; ft.ft_month=local->tm_mon+1; ft.ft_day=local->tm_mday;
  ft.ft_hour=local->tm_hour; ft.ft_min=local->tm_min; ft.ft_tsec=local->tm_sec/2;
  return setftime(fd,&ft);
}
int utimensat(int fd,const char *path,const struct timespec t[2],int flags)
{
  time_t value; struct stat st; struct utimbuf times;
  int changed;
  if(fd!=AT_FDCWD || (flags && flags!=AT_SYMLINK_NOFOLLOW)) { errno=ENOSYS; return -1; }
  changed=modificationTime(t,&value); if(changed<0)return -1;
  if(stat(path,&st))return -1;
  warnAccessTime(t);
  if(!changed)return 0;
  times.actime=st.st_atime;times.modtime=value;
  return utime(path,&times);
}

/* Refuse DOS filename aliasing before opening or creating files. */
int z7_dos_validate_path(const char *path)
{
  const char *p=path;
  if (_use_lfn(path)) return 1;
  if (isalpha((unsigned char)p[0]) && p[1]==':') p+=2;
  while (*p) {
    const char *start;
    unsigned base=0,ext=0,dot=0;
    char device[9];
    while (*p=='/' || *p=='\\') ++p;
    if (!*p) break;
    start=p;
    while (*p && *p!='/' && *p!='\\') ++p;
    if ((p-start==1 && start[0]=='.') || (p-start==2 && start[0]=='.' && start[1]=='.')) continue;
    while(start<p) {
      unsigned char ch=(unsigned char)*start++;
      if(ch=='.') { if(dot || !base)goto invalid; dot=1;continue; }
      if(ch<=32 || strchr("\"*+,/:;<=>?[\\]|",ch))goto invalid;
      if(dot) { if(++ext>3)goto invalid; }
      else { if(base==8)goto invalid; device[base++]=(char)toupper(ch); }
    }
    if(dot&&!ext)goto invalid;
    device[base]=0;
    if(!strcmp(device,"CON")||!strcmp(device,"PRN")||!strcmp(device,"AUX")||!strcmp(device,"NUL")||!strcmp(device,"CLOCK$"))goto invalid;
    if(base==4 && device[3]>='0' && device[3]<='9' && (!memcmp(device,"COM",3)||!memcmp(device,"LPT",3)))goto invalid;
  }
  return 1;
invalid:
  fprintf(stderr,"ERROR: DOS cannot represent this path without changing its name: %s\n",path);
  errno=ENAMETOOLONG;return 0;
}
