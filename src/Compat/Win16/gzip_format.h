/* RFC 1952 gzip single-member container over the existing RFC 1951 Deflate
   codec. Create reuses Z7Win16_DeflateEncode; extract/test reuse
   Z7Win16_Inflate (which returns the finalized CRC-32). One member per file,
   as upstream gzip. Shared by the CLI and the GUI front ends.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_GZIP_FORMAT_H
#define Z7_WIN16_GZIP_FORMAT_H
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../../C/7zTypes.h"
#include "../../C/7zCrc.h"
#include "deflate_encode.h"
#include "deflate_stream.h"
#include "dos_paths.h"

static char gzipMessage[256];

/* ---- little-endian helpers ---- */
static void gzPut32(Byte *p,UInt32 v){p[0]=(Byte)v;p[1]=(Byte)(v>>8);p[2]=(Byte)(v>>16);p[3]=(Byte)(v>>24);}
static UInt32 gzGet32(const Byte *p){return (UInt32)p[0]|((UInt32)p[1]<<8)|((UInt32)p[2]<<16)|((UInt32)p[3]<<24);}

/* ---- streams ---- */
typedef struct { ISeqInStream vt; FILE *f; UInt32 crc; UInt32 size; } GzSrc;
static SRes gzSrcRead(ISeqInStreamPtr pp,void *buf,size_t *size){
 GzSrc *s=(GzSrc *)pp;size_t got=fread(buf,1,*size,s->f);
 if(ferror(s->f)){*size=0;return SZ_ERROR_READ;}
 if(got){s->crc=CrcUpdate(s->crc,buf,got);s->size+=(UInt32)got;}
 *size=got;return SZ_OK;
}
typedef struct { ISeqOutStream vt; FILE *f; SRes err; } GzDst;
static size_t gzDstWrite(ISeqOutStreamPtr pp,const void *buf,size_t size){
 GzDst *s=(GzDst *)pp;size_t n=fwrite(buf,1,size,s->f);if(n!=size)s->err=SZ_ERROR_WRITE;return n;
}
typedef struct { ISeekInStream vt; FILE *f; } GzIn;
static SRes gzInRead(ISeekInStreamPtr pp,void *buf,size_t *size){
 GzIn *s=(GzIn *)pp;size_t got=fread(buf,1,*size,s->f);
 if(ferror(s->f)){*size=0;return SZ_ERROR_READ;}*size=got;return SZ_OK;
}
static SRes gzInSeek(ISeekInStreamPtr pp,Int64 *pos,ESzSeek origin){
 GzIn *s=(GzIn *)pp;long n=(long)*pos;if((Int64)n!=*pos)return SZ_ERROR_UNSUPPORTED;
 if(fseek(s->f,n,(int)origin))return SZ_ERROR_READ;n=ftell(s->f);if(n<0)return SZ_ERROR_READ;*pos=n;return SZ_OK;
}

/* ---- name helpers ---- */
static int gzipIsName(const char *path){size_t n=strlen(path);return n>3&&sameName(path+n-3,".GZ");}
static int gzipMagicFile(FILE *f){int a,b;if(fseek(f,0,SEEK_SET))return 0;a=fgetc(f);b=fgetc(f);if(fseek(f,0,SEEK_SET))return 0;return a==0x1f&&b==0x8b;}
/* Derive the member name from the archive basename by removing one .GZ suffix. */
static void gzipDefaultName(const char *archive,char *out,size_t max){
 const char *b=archive,*p;size_t n;
 for(p=archive;*p;p++)if(*p=='\\'||*p=='/'||*p==':')b=p+1;
 n=strlen(b);if(n>3&&sameName(b+n-3,".GZ"))n-=3;
 if(n>=max)n=max-1;memcpy(out,b,n);out[n]=0;
 if(!out[0]){strncpy(out,"data",max-1);out[max-1]=0;}
}

/* ---- header parse / write ---- */
static SRes gzipParse(FILE *f,UInt32 *headerLen,char *nameOut,size_t nameMax){
 Byte h[10];int flg;UInt32 pos=10;
 if(fseek(f,0,SEEK_SET))return SZ_ERROR_READ;
 if(fread(h,1,10,f)!=10)return SZ_ERROR_NO_ARCHIVE;
 if(h[0]!=0x1f||h[1]!=0x8b||h[2]!=8)return SZ_ERROR_NO_ARCHIVE;
 flg=h[3];if(nameOut&&nameMax)nameOut[0]=0;
 if(flg&4){Byte x[2];UInt32 xl;if(fread(x,1,2,f)!=2)return SZ_ERROR_DATA;xl=(UInt32)x[0]|((UInt32)x[1]<<8);if(fseek(f,(long)xl,SEEK_CUR))return SZ_ERROR_READ;pos+=2+xl;}
 if(flg&8){int c;size_t k=0;for(;;){c=fgetc(f);if(c==EOF)return SZ_ERROR_DATA;pos++;if(!c)break;if(nameOut&&k+1<nameMax&&c>0&&c<127)nameOut[k++]=(char)c;}if(nameOut&&nameMax)nameOut[k<nameMax?k:nameMax-1]=0;}
 if(flg&16){int c;for(;;){c=fgetc(f);if(c==EOF)return SZ_ERROR_DATA;pos++;if(!c)break;}}
 if(flg&2){if(fseek(f,2,SEEK_CUR))return SZ_ERROR_READ;pos+=2;}
 *headerLen=pos;return SZ_OK;
}
static SRes gzipTrailer(FILE *f,UInt32 *crc,UInt32 *isize,UInt32 *fileSize){
 long end;Byte t[8];
 if(fseek(f,0,SEEK_END))return SZ_ERROR_READ;end=ftell(f);if(end<0)return SZ_ERROR_READ;*fileSize=(UInt32)end;
 if(end<18)return SZ_ERROR_DATA;
 if(fseek(f,end-8,SEEK_SET))return SZ_ERROR_READ;
 if(fread(t,1,8,f)!=8)return SZ_ERROR_READ;
 *crc=gzGet32(t);*isize=gzGet32(t+4);return SZ_OK;
}

/* ---- create (single source file) ---- */
static SRes gzipCreate(const char *path,const char *source,ISzAllocPtr alloc){
 FILE *in,*out;GzSrc src;GzDst dst;SRes r;Byte trailer[8];struct stat st;
 static const Byte head[10]={0x1f,0x8b,0x08,0x00,0,0,0,0,0x00,0xff};
 gzipMessage[0]=0;
 if(!stat(path,&st)){strcpy(gzipMessage,"ERROR: Existing archives are never overwritten.");return SZ_ERROR_PARAM;}
 if(stat(source,&st)||(st.st_mode&S_IFDIR)){strcpy(gzipMessage,"ERROR: gzip stores exactly one regular file.");return SZ_ERROR_PARAM;}
 in=fopen(source,"rb");if(!in){strcpy(gzipMessage,"ERROR: Cannot open the input file.");return SZ_ERROR_READ;}
 out=fopen(path,"wb");if(!out){fclose(in);strcpy(gzipMessage,"ERROR: Cannot create the archive.");return SZ_ERROR_WRITE;}
 r=fwrite(head,1,10,out)==10?SZ_OK:SZ_ERROR_WRITE;
 if(r==SZ_OK){
  src.vt.Read=gzSrcRead;src.f=in;src.crc=CRC_INIT_VAL;src.size=0;
  dst.vt.Write=gzDstWrite;dst.f=out;dst.err=SZ_OK;
  r=Z7Win16_DeflateEncode(&src.vt,&dst.vt,alloc);
  if(r==SZ_OK)r=dst.err;
 }
 if(r==SZ_OK){gzPut32(trailer,CRC_GET_DIGEST(src.crc));gzPut32(trailer+4,src.size);if(fwrite(trailer,1,8,out)!=8)r=SZ_ERROR_WRITE;}
 fclose(in);if(fclose(out)&&r==SZ_OK)r=SZ_ERROR_WRITE;
 if(r!=SZ_OK){remove(path);if(!gzipMessage[0])strcpy(gzipMessage,"ERROR: Could not write the gzip archive.");}
 return r;
}

/* ---- inflate the single member to an output stream ---- */
static SRes gzipDecodeTo(FILE *f,ISeqOutStreamPtr out,ISzAllocPtr alloc,UInt32 *outSize){
 UInt32 headerLen,crcStored,isize,fileSize,crcGot,packed;GzIn in;SRes r;
 r=gzipParse(f,&headerLen,NULL,0);if(r!=SZ_OK)return r;
 r=gzipTrailer(f,&crcStored,&isize,&fileSize);if(r!=SZ_OK)return r;
 if(fileSize<headerLen+8)return SZ_ERROR_DATA;
 packed=fileSize-headerLen-8;
 if(fseek(f,(long)headerLen,SEEK_SET))return SZ_ERROR_READ;
 in.vt.Read=gzInRead;in.vt.Seek=gzInSeek;in.f=f;
 r=Z7Win16_Inflate(&in.vt,out,(UInt64)packed,(UInt64)isize,alloc,&crcGot);
 if(r!=SZ_OK)return r;
 if(crcGot!=crcStored)return SZ_ERROR_CRC;
 if(outSize)*outSize=isize;
 return SZ_OK;
}

/* ---- test ---- */
typedef struct { ISeqOutStream vt; UInt32 size; } GzNull;
static size_t gzNullWrite(ISeqOutStreamPtr pp,const void *buf,size_t size){GzNull *s=(GzNull *)pp;(void)buf;s->size+=(UInt32)size;return size;}
static SRes gzipTest(FILE *f,ISzAllocPtr alloc){
 GzNull n;SRes r;n.vt.Write=gzNullWrite;n.size=0;
 gzipMessage[0]=0;r=gzipDecodeTo(f,&n.vt,alloc,NULL);
 if(r!=SZ_OK&&!gzipMessage[0])strcpy(gzipMessage,"ERROR: The gzip archive is damaged.");
 return r;
}

/* ---- list ---- */
static SRes gzipList(FILE *f,const char *archivePath){
 UInt32 headerLen,crcStored,isize,fileSize;char name[260];SRes r;
 gzipMessage[0]=0;
 r=gzipParse(f,&headerLen,name,sizeof(name));if(r!=SZ_OK)return r;
 r=gzipTrailer(f,&crcStored,&isize,&fileSize);if(r!=SZ_OK)return r;
 if(!name[0])gzipDefaultName(archivePath,name,sizeof(name));
 printf("F %lu %s\n",(unsigned long)isize,name);
 return SZ_OK;
}

/* ---- extract (single member into destDir) ---- */
static SRes gzipExtract(FILE *f,const char *archivePath,const char *destDir,ISzAllocPtr alloc){
 UInt32 headerLen,crcStored,isize,fileSize;char name[260],path[300];GzDst dst;FILE *out;SRes r;struct stat st;size_t dl,nl;
 gzipMessage[0]=0;
 r=gzipParse(f,&headerLen,name,sizeof(name));if(r!=SZ_OK)return r;
 r=gzipTrailer(f,&crcStored,&isize,&fileSize);if(r!=SZ_OK)return r;
 if(!name[0])gzipDefaultName(archivePath,name,sizeof(name));
 if(!validDosComponent(name,strlen(name))){strcpy(gzipMessage,"ERROR: The stored name is not a valid DOS file name.");return SZ_ERROR_UNSUPPORTED;}
 if(Z7_MKDIR(destDir)&&(stat(destDir,&st)||!(st.st_mode&S_IFDIR))){strcpy(gzipMessage,"ERROR: Cannot create the output directory.");return SZ_ERROR_WRITE;}
 dl=strlen(destDir);nl=strlen(name);if(dl+1+nl+1>sizeof(path)){strcpy(gzipMessage,"ERROR: The output path is too long.");return SZ_ERROR_UNSUPPORTED;}
 memcpy(path,destDir,dl);path[dl]=Z7_PATH_SEPARATOR;memcpy(path+dl+1,name,nl+1);
 if(!stat(path,&st)){strcpy(gzipMessage,"ERROR: The destination already exists. No output was created.");return SZ_ERROR_PARAM;}
 out=fopen(path,"wb");if(!out){strcpy(gzipMessage,"ERROR: Cannot create the output file.");return SZ_ERROR_WRITE;}
 dst.vt.Write=gzDstWrite;dst.f=out;dst.err=SZ_OK;
 r=gzipDecodeTo(f,&dst.vt,alloc,NULL);
 if(r==SZ_OK)r=dst.err;
 if(fclose(out)&&r==SZ_OK)r=SZ_ERROR_WRITE;
 if(r!=SZ_OK){remove(path);if(!gzipMessage[0])strcpy(gzipMessage,"ERROR: The gzip archive is damaged.");}
 return r;
}
#endif
