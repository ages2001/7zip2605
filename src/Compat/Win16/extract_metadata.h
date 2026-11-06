/* Apply representable metadata only after all extracted data has passed CRC
   validation. Unsupported metadata remains in the unmodified source archive. */
#ifndef Z7_WIN16_EXTRACT_METADATA_H
#define Z7_WIN16_EXTRACT_METADATA_H
#include <time.h>
#include <utime.h>
#ifdef __WATCOMC__
#include <dos.h>
#endif
#define META_TIME 1
#define META_ATTR 2
#define META_EXTRA 4
#define META_DIR_TIME 8
static unsigned extractMetadata(const CSzArEx *archive,UInt32 index,const char *path){
 unsigned warning=0;
 if(SzBitWithVals_Check(&archive->MTime,index)){
  const CNtfsFileTime *ft=&archive->MTime.Vals[index];
  UInt64 ticks=((UInt64)ft->High<<32)|ft->Low,seconds=ticks/10000000UL;
  struct utimbuf times;time_t converted;
  /* DOS dates start in 1980. Reject rather than wrap an unrepresentable date. */
  if(seconds<11960006400ULL)warning|=META_TIME;
  else {
   seconds-=11644473600ULL;converted=(time_t)seconds;
   if((UInt64)converted!=seconds||seconds>4354819199ULL)warning|=META_TIME;
   else {
    times.actime=times.modtime=converted;
    if(utime(path,&times))warning|=META_TIME;
    if(ticks%20000000UL)warning|=META_TIME; /* FAT has two-second precision. */
   }
  }
 }
 if(SzBitWithVals_Check(&archive->Attribs,index)){
  UInt32 attributes=archive->Attribs.Vals[index];
  if(attributes&~(UInt32)0x37)warning|=META_ATTR;
#ifdef __WATCOMC__
  if(_dos_setfileattr(path,(unsigned)(attributes&0x27)))warning|=META_ATTR;
#else
  struct stat info;
  if(stat(path,&info)||chmod(path,(attributes&1)?info.st_mode&~(S_IWUSR|S_IWGRP|S_IWOTH):info.st_mode|S_IWUSR))warning|=META_ATTR;
  if(attributes&6)warning|=META_ATTR;
#endif
 }
 if(SzBitWithVals_Check(&archive->CTime,index))warning|=META_EXTRA;
 if((warning&META_TIME)&&SzArEx_IsDir(archive,index)){warning&=~META_TIME;warning|=META_DIR_TIME;}
 return warning;
}
#endif
