/* Classic ZIP container reader for the native segmented backend.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#include <string.h>
#include <time.h>
#include "zip_archive.h"
static unsigned zip16(const Byte *p){return p[0]|((unsigned)p[1]<<8);}
static UInt32 zip32(const Byte *p){return (UInt32)zip16(p)|((UInt32)zip16(p+2)<<16);}
static SRes zipRead(ISeekInStreamPtr in,UInt64 position,void *data,size_t size){
 Int64 pos=(Int64)position;Byte *p=(Byte *)data;
 if(pos<0)return SZ_ERROR_UNSUPPORTED;
 RINOK(ISeekInStream_Seek(in,&pos,SZ_SEEK_SET))
 while(size){size_t n=size;RINOK(ISeekInStream_Read(in,p,&n))if(!n)return SZ_ERROR_INPUT_EOF;p+=n;size-=n;}
 return SZ_OK;
}
static void *zipAlloc(ISzAllocPtr alloc,UInt32 count,unsigned unit){
 UInt32 size=count*unit;void *p;
 if(!count)size=1;
 if(size>65520UL)return NULL;
 p=ISzAlloc_Alloc(alloc,(size_t)size);if(p)memset(p,0,(size_t)size);return p;
}
/* ZIP's legacy filename encoding is CP437, independent of the host locale. */
static const UInt16 cp437[128]={
 0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
 0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,0x00ff,0x00d6,0x00dc,0x00a2,0x00a3,0x00a5,0x20a7,0x0192,
 0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,0x00bf,0x2310,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
 0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,
 0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567,
 0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256b,0x256a,0x2518,0x250c,0x2588,0x2584,0x258c,0x2590,0x2580,
 0x03b1,0x00df,0x0393,0x03c0,0x03a3,0x03c3,0x00b5,0x03c4,0x03a6,0x0398,0x03a9,0x03b4,0x221e,0x03c6,0x03b5,0x2229,
 0x2261,0x00b1,0x2265,0x2264,0x2320,0x2321,0x00f7,0x2248,0x00b0,0x2219,0x00b7,0x221a,0x207f,0x00b2,0x25a0,0x00a0};
static SRes zipName(const Byte *src,unsigned n,int utf,Byte *dest,unsigned *units){
 unsigned i=0,o=0;
 while(i<n){
  UInt32 ch=src[i++];unsigned more=0,j;
  if(!ch)return SZ_ERROR_ARCHIVE;
  if(ch>=128){
   if(!utf)ch=cp437[ch-128];
   else {
    UInt32 minimum;
    if(ch>=0xc2&&ch<=0xdf){more=1;ch&=31;minimum=0x80;}
    else if(ch>=0xe0&&ch<=0xef){more=2;ch&=15;minimum=0x800;}
    else if(ch>=0xf0&&ch<=0xf4){more=3;ch&=7;minimum=0x10000;}
    else return SZ_ERROR_ARCHIVE;
    if(more>n-i)return SZ_ERROR_ARCHIVE;
    for(j=0;j<more;j++){unsigned c=src[i++];if((c&0xc0)!=0x80)return SZ_ERROR_ARCHIVE;ch=(ch<<6)|(c&63);}
    if(ch<minimum||ch>0x10ffffUL||(ch>=0xd800&&ch<=0xdfff))return SZ_ERROR_ARCHIVE;
   }
  }
  if(ch>=0x10000UL){UInt32 v=ch-0x10000UL;unsigned high=0xd800+(unsigned)(v>>10);dest[2*o]=(Byte)high;dest[2*o+1]=(Byte)(high>>8);o++;ch=0xdc00+(v&1023);}
  dest[2*o]=(Byte)ch;dest[2*o+1]=(Byte)(ch>>8);o++;
 }
 dest[2*o]=dest[2*o+1]=0;*units=o+1;return SZ_OK;
}
static void zipTime(CSzArEx *a,UInt32 i,unsigned date,unsigned time){
 struct tm t,original;time_t seconds;UInt64 ticks;
 memset(&t,0,sizeof(t));t.tm_year=80+(date>>9);t.tm_mon=((date>>5)&15)-1;t.tm_mday=date&31;
 t.tm_hour=time>>11;t.tm_min=(time>>5)&63;t.tm_sec=(time&31)*2;t.tm_isdst=-1;original=t;
 seconds=mktime(&t);
 if(seconds==(time_t)-1||t.tm_year!=original.tm_year||t.tm_mon!=original.tm_mon||t.tm_mday!=original.tm_mday||t.tm_hour!=original.tm_hour||t.tm_min!=original.tm_min||t.tm_sec!=original.tm_sec){
  /* Invalid dates remain unrepresentable rather than being silently normalized. */
  a->MTime.Vals[i].Low=0;a->MTime.Vals[i].High=0;
 }else{ticks=((UInt64)seconds+11644473600ULL)*10000000UL;a->MTime.Vals[i].Low=(UInt32)ticks;a->MTime.Vals[i].High=(UInt32)(ticks>>32);}
 a->MTime.Defs[i>>3]|=0x80>>(i&7);
}
/* Validate TLV bounds even for fields that cannot be applied on DOS. Unicode
   path overrides require their own CRC-aware interpretation, so reject them
   explicitly instead of extracting a different name. ZIP64 is separate work. */
static SRes zipExtras(ISeekInStreamPtr in,UInt64 position,unsigned bytes,unsigned *warning,UInt32 *mtime,int *hasTime){
 while(bytes){
  Byte h[4];unsigned id,n;
  if(bytes<4)return SZ_ERROR_ARCHIVE;
  RINOK(zipRead(in,position,h,4))id=zip16(h);n=zip16(h+2);
  if(n>bytes-4)return SZ_ERROR_ARCHIVE;
  if(id==1 || id==0x7075)return SZ_ERROR_UNSUPPORTED;
  if(id==0x5455&&n==5){
   Byte stamp[5];RINOK(zipRead(in,position+4,stamp,5))
   if(stamp[0]==1){UInt32 value=zip32(stamp+1);if(*hasTime&&*mtime!=value)return SZ_ERROR_ARCHIVE;*mtime=value;*hasTime=1;}
   else *warning=1;
  }else *warning=1;
  position+=4+(UInt32)n;bytes-=4+n;
 }
 return SZ_OK;
}
static SRes zipOpen(CSzArEx *a,ISeekInStreamPtr in,ISzAllocPtr alloc,unsigned *extraMetadata){
 Byte end[22],h[46],local[30],*name=NULL,*other=NULL;
 UInt64 length,eocd,central,centralSize,cursor,total=0;
 Int64 pos=0;UInt32 count,i,names=0;unsigned nameUnits=0;SRes result=SZ_OK;
 RINOK(ISeekInStream_Seek(in,&pos,SZ_SEEK_END))if(pos<22)return SZ_ERROR_NO_ARCHIVE;length=(UInt64)pos;
 /* Scan backward in 512-byte chunks with a 21-byte overlap for headers. */
 {Byte tail[533];UInt64 high=length-22,low,min=high>65535UL?high-65535UL:0;int found=0;
  while(!found){
   unsigned k;low=high-min>511?high-511:min;
   RINOK(zipRead(in,low,tail,(size_t)(high-low)+22))
   k=(unsigned)(high-low)+1;
   while(k--){if(zip32(tail+k)==0x06054b50UL && low+k+22+zip16(tail+k+20)==length){eocd=low+k;memcpy(end,tail+k,22);found=1;break;}}
   if(found)break;if(low==min)return SZ_ERROR_NO_ARCHIVE;high=low-1;
  }
 }
 if(zip16(end+4)||zip16(end+6)||zip16(end+8)!=zip16(end+10))return SZ_ERROR_UNSUPPORTED;
 count=zip16(end+10);centralSize=zip32(end+12);central=zip32(end+16);
 if(count==65535UL||centralSize==0xffffffffUL||central==0xffffffffUL)return SZ_ERROR_UNSUPPORTED;
 if(central+centralSize!=eocd)return SZ_ERROR_ARCHIVE;
 if(count>4095)return SZ_ERROR_MEM;
 cursor=central;
 for(i=0;i<count;i++){
  unsigned n,extra,comment;
  if(cursor+46>eocd)return SZ_ERROR_ARCHIVE;
  RINOK(zipRead(in,cursor,h,46))if(zip32(h)!=0x02014b50UL)return SZ_ERROR_ARCHIVE;
  n=zip16(h+28);extra=zip16(h+30);comment=zip16(h+32);
  if(!n)return SZ_ERROR_ARCHIVE;
  names+=(UInt32)n+1;if(names>32760UL)return SZ_ERROR_MEM;
  cursor+=46+(UInt32)n+extra+comment;if(cursor>eocd)return SZ_ERROR_ARCHIVE;
 }
 if(cursor!=eocd)return SZ_ERROR_ARCHIVE;
 a->NumFiles=count;a->db.NumFolders=count;a->db.NumPackStreams=count*2;
#define ZA(field,num,type) a->field=(type *)zipAlloc(alloc,num,sizeof(type));if(!a->field)return SZ_ERROR_MEM
 ZA(UnpackPositions,count+1,UInt64);ZA(IsDirs,(count+7)/8,Byte);
 ZA(CRCs.Defs,(count+7)/8,Byte);ZA(CRCs.Vals,count,UInt32);
 ZA(Attribs.Defs,(count+7)/8,Byte);ZA(Attribs.Vals,count,UInt32);
 ZA(MTime.Defs,(count+7)/8,Byte);ZA(MTime.Vals,count,CNtfsFileTime);
 ZA(FolderToFile,count+1,UInt32);ZA(FileToFolder,count,UInt32);
 ZA(FileNameOffsets,count+1,size_t);ZA(FileNames,names*2,Byte);
 ZA(db.PackPositions,count*2+1,UInt64);ZA(db.FoCodersOffsets,count+1,size_t);
 ZA(db.FoStartPackStreamIndex,count+1,UInt32);ZA(db.FoToCoderUnpackSizes,count+1,UInt32);
 ZA(db.FoToMainUnpackSizeIndex,count,Byte);ZA(db.CoderUnpackSizes,count,UInt64);
 ZA(db.CodersData,count*5,Byte);
#undef ZA
 /* Names are bounded by the aggregate metadata allocation, before conversion. */
 name=(Byte *)zipAlloc(alloc,names+1,1);other=(Byte *)zipAlloc(alloc,names+1,1);
 if(!name||!other){result=SZ_ERROR_MEM;goto done;}
 cursor=central;
 for(i=0;i<count;i++){
  unsigned n,extra,comment,flags,method,directory,units,localName,localExtra;
  UInt32 packed,unpacked,crc,offset,j,unixTime=0;int hasUnixTime=0;UInt64 dataStart,dataEnd,entryEnd;
#define ZR(call) result=(call);if(result!=SZ_OK)goto done
  ZR(zipRead(in,cursor,h,46));n=zip16(h+28);extra=zip16(h+30);comment=zip16(h+32);
  flags=zip16(h+8);method=zip16(h+10);crc=zip32(h+16);packed=zip32(h+20);unpacked=zip32(h+24);offset=zip32(h+42);
  if(zip16(h+34)||packed==0xffffffffUL||unpacked==0xffffffffUL||offset==0xffffffffUL||(flags&~0x080eU)||(method!=0&&method!=8&&method!=9)){result=SZ_ERROR_UNSUPPORTED;goto done;}
  if((method==0&&packed!=unpacked)||offset>=central){result=SZ_ERROR_ARCHIVE;goto done;}
  ZR(zipRead(in,cursor+46,name,n));directory=name[n-1]=='/';
  ZR(zipExtras(in,cursor+46+n,extra,extraMetadata,&unixTime,&hasUnixTime));
  if(directory&&unpacked){result=SZ_ERROR_ARCHIVE;goto done;}
  ZR(zipRead(in,offset,local,30));localName=zip16(local+26);localExtra=zip16(local+28);
  if(zip32(local)!=0x04034b50UL||zip16(local+6)!=flags||zip16(local+8)!=method||localName!=n){result=SZ_ERROR_ARCHIVE;goto done;}
  dataStart=(UInt64)offset+30+localName+localExtra;dataEnd=dataStart+packed;entryEnd=dataEnd;
  if(dataEnd>central){result=SZ_ERROR_ARCHIVE;goto done;}
  ZR(zipExtras(in,(UInt64)offset+30+localName,localExtra,extraMetadata,&unixTime,&hasUnixTime));
  ZR(zipRead(in,(UInt64)offset+30,other,n));if(memcmp(name,other,n)){result=SZ_ERROR_ARCHIVE;goto done;}
  if(!(flags&8)){
   if(zip32(local+14)!=crc||zip32(local+18)!=packed||zip32(local+22)!=unpacked){result=SZ_ERROR_ARCHIVE;goto done;}
  }else{
   Byte desc[16];unsigned skip=0;
   if(dataEnd+12>central){result=SZ_ERROR_ARCHIVE;goto done;}
   ZR(zipRead(in,dataEnd,desc,12));if(!(zip32(desc)==crc && zip32(desc+4)==packed && zip32(desc+8)==unpacked) && zip32(desc)==0x08074b50UL){skip=4;if(dataEnd+16>central){result=SZ_ERROR_ARCHIVE;goto done;}ZR(zipRead(in,dataEnd+12,desc+12,4));}
   if(zip32(desc+skip)!=crc||zip32(desc+skip+4)!=packed||zip32(desc+skip+8)!=unpacked){result=SZ_ERROR_ARCHIVE;goto done;}
   entryEnd+=12+skip;
  }
  /* The spare pack-position slot records the next entry's start, so use local
     header offsets retained temporarily in FileToFolder for overlap checking. */
  for(j=0;j<i;j++)if((UInt64)offset<a->db.PackPositions[j*2+1] && entryEnd>a->FileToFolder[j]){result=SZ_ERROR_ARCHIVE;goto done;}
  a->FileToFolder[i]=offset;
  a->db.PackPositions[i*2]=dataStart;a->db.PackPositions[i*2+1]=entryEnd;
  ZR(zipName(name,n-directory,(flags&0x800)!=0,a->FileNames+2*nameUnits,&units));
  a->FileNameOffsets[i]=nameUnits;nameUnits+=units;
  a->UnpackPositions[i]=total;total+=unpacked;
  a->CRCs.Defs[i>>3]|=0x80>>(i&7);a->CRCs.Vals[i]=crc;
  a->Attribs.Defs[i>>3]|=0x80>>(i&7);a->Attribs.Vals[i]=zip32(h+38)&0xffffUL;
  if(directory){a->IsDirs[i>>3]|=0x80>>(i&7);a->Attribs.Vals[i]|=16;}
  /* UNIX mode information is preserved in the source and explicitly reported
     as unrepresentable by the shared metadata applicator. */
  if((zip16(h+4)>>8)==3 && (zip32(h+38)>>16))a->Attribs.Vals[i]|=0x80000000UL;
  zipTime(a,i,zip16(h+14),zip16(h+12));
  if(hasUnixTime){UInt64 ticks=(UInt64)((Int64)(Int32)unixTime+11644473600LL)*10000000UL;a->MTime.Vals[i].Low=(UInt32)ticks;a->MTime.Vals[i].High=(UInt32)(ticks>>32);}
  a->FolderToFile[i]=i;a->db.FoCodersOffsets[i]=(size_t)i*5;
  a->db.FoStartPackStreamIndex[i]=i*2;a->db.FoToCoderUnpackSizes[i]=i;a->db.CoderUnpackSizes[i]=unpacked;
  {Byte *c=a->db.CodersData+(size_t)i*5;c[0]=1;if(method==8||method==9){c[1]=3;c[2]=4;c[3]=1;c[4]=(Byte)method;}else{c[1]=1;c[2]=0;}}
  cursor+=46+(UInt32)n+extra+comment;
#undef ZR
 }
 /* Compact coder descriptions; restore pack ends without descriptor bytes. */
 {size_t used=0;for(i=0;i<count;i++){
  unsigned size=a->db.CodersData[(size_t)i*5+1]==3?5:3;
  memmove(a->db.CodersData+used,a->db.CodersData+(size_t)i*5,size);a->db.FoCodersOffsets[i]=used;used+=size;
  /* Re-read the matching central record so no extra segment-sized arrays are needed. */
 }a->db.FoCodersOffsets[count]=used;}
 cursor=central;
 for(i=0;i<count;i++){
  result=zipRead(in,cursor,h,46);if(result!=SZ_OK)goto done;
  a->db.PackPositions[i*2+1]=a->db.PackPositions[i*2]+zip32(h+20);a->FileToFolder[i]=i;
  cursor+=46+(UInt32)zip16(h+28)+zip16(h+30)+zip16(h+32);
 }
 a->UnpackPositions[count]=total;a->FileNameOffsets[count]=nameUnits;a->FolderToFile[count]=count;
 a->db.FoStartPackStreamIndex[count]=count*2;a->db.FoToCoderUnpackSizes[count]=count;
 a->db.RangeLimit=length;
done:
 ISzAlloc_Free(alloc,name);ISzAlloc_Free(alloc,other);return result;
}
SRes Z7Win16_OpenZip(CSzArEx *a,ISeekInStreamPtr in,ISzAllocPtr alloc,unsigned *extraMetadata){
 SRes result;*extraMetadata=0;result=zipOpen(a,in,alloc,extraMetadata);if(result!=SZ_OK)SzArEx_Free(a,alloc);return result;
}
