/* Native DJGPP regression for GMT fallback, configured zones and DOS times. */
#include "../runtime.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
static unsigned checks, failures;
static void check(int ok, const char *what) {
 ++checks; if (!ok) { ++failures; printf("FAIL %s (errno=%d)\n", what, errno); }
}
static const struct Case { int year,month,day,hour,minute,second; unsigned long utc; } cases[] = {
 {1980,1,1,0,0,0,315532800UL}, {2000,2,29,12,34,56,951827696UL},
 {2038,1,19,3,14,8,2147483648UL}, {2105,12,31,23,59,58,4291747198UL}
};
static void run(const char *zone, unsigned long offset) {
 unsigned i; struct tm t, *back; struct timespec ts[2]; struct stat st;
 char path[13]; int fd;
 if (zone) setenv("TZ",zone,1); else unsetenv("TZ"); tzset();
 for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
  const struct Case *c=cases+i; time_t value;
  memset(&t,0,sizeof(t));t.tm_year=c->year-1900;t.tm_mon=c->month-1;t.tm_mday=c->day;
  t.tm_hour=c->hour;t.tm_min=c->minute;t.tm_sec=c->second;t.tm_isdst=-1;
  value=mktime(&t);check(value==c->utc+offset,"mktime UTC seconds");
  back=localtime(&value);check(back && back->tm_year==c->year-1900 && back->tm_mon==c->month-1 && back->tm_mday==c->day && back->tm_hour==c->hour && back->tm_min==c->minute && back->tm_sec==c->second,"local calendar round trip");
  sprintf(path,"STAMP%u.TXT",i);fd=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0666);check(fd>=0,"open timestamp file");if(fd<0)continue;
  check(write(fd,"timestamp\r\n",11)==11,"write timestamp file");
  ts[0].tv_sec=0;ts[0].tv_nsec=UTIME_OMIT;ts[1].tv_sec=value;ts[1].tv_nsec=0;
  check(futimens(fd,ts)==0,"set file modification time");check(close(fd)==0,"close timestamp file");
  check(stat(path,&st)==0 && st.st_mtime==value,"stored DOS timestamp");
  check(utimensat(AT_FDCWD,path,ts,0)==0,"set path modification time");
  check(stat(path,&st)==0 && st.st_mtime==value,"stored path timestamp");
 }
 printf("Tested zone: %s\n",zone?zone:"unset (GMT fallback)");
}
int main(void) {
 struct tm t;time_t v; struct tm *g;
 run(NULL,0);run("UTC0",0);run("EST5",18000);run("<BAD>",0);
 setenv("TZ","EST5EDT,M3.2.0,M11.1.0",1);tzset();memset(&t,0,sizeof(t));
 t.tm_year=100;t.tm_mon=6;t.tm_mday=1;t.tm_hour=12;t.tm_isdst=-1;
 v=mktime(&t);check(v==962467200UL,"configured daylight saving conversion");
 g=gmtime(&v);check(g && g->tm_hour==16,"GMT independent of local daylight saving");
 run(NULL,0); /* Leave files with known UTC values for independent archive checks. */
 printf("%s DOS time tests: %u checks, %u failures\n",failures?"FAIL":"PASS",checks,failures);
 return failures?2:0;
}
