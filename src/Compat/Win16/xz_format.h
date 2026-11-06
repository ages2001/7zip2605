/* .xz container decode over the existing LZMA2 decoder (Lzma2Dec, which uses
   the paged dictionary in real mode, so large xz dictionaries decode by
   paging). Parses the stream header, one or more blocks (LZMA2 filter only)
   and drives the decoder to its end marker. The CRC32 stream check is verified;
   other check types rely on the LZMA2 end-of-stream marker and are confirmed by
   byte-exact output. Read side only (create is a separate concern).
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_XZ_FORMAT_H
#define Z7_WIN16_XZ_FORMAT_H
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../../C/7zTypes.h"
#include "../../C/7zCrc.h"
#include "../../C/Lzma2Dec.h"
#ifdef Z7_WIN16_WRITE_XZ_AES
#include "../../C/LzmaEnc.h"
#endif
#include "dos_paths.h"

static char xzMessage[256];
#define XZ_CHUNK 2048

static int xzIsName(const char *path){size_t n=strlen(path);return n>3&&sameName(path+n-3,".XZ");}
static int xzMagicFile(FILE *f){Byte m[6];if(fseek(f,0,SEEK_SET))return 0;if(fread(m,1,6,f)!=6){fseek(f,0,SEEK_SET);return 0;}fseek(f,0,SEEK_SET);return m[0]==0xFD&&m[1]=='7'&&m[2]=='z'&&m[3]=='X'&&m[4]=='Z'&&m[5]==0;}
static void xzDefaultName(const char *archive,char *out,size_t max){
 const char *b=archive,*p;size_t n;
 for(p=archive;*p;p++)if(*p=='\\'||*p=='/'||*p==':')b=p+1;
 n=strlen(b);if(n>3&&sameName(b+n-3,".XZ"))n-=3;
 if(n>=max)n=max-1;memcpy(out,b,n);out[n]=0;if(!out[0]){strncpy(out,"data",max-1);out[max-1]=0;}
}

/* variable-length integer (base-128 LE); returns bytes used, 0 on error */
static int xzVli(const Byte *p,size_t avail,UInt32 *value){
 UInt32 v=0;int i;for(i=0;i<9&&(size_t)i<avail;i++){v|=(UInt32)(p[i]&0x7F)<<(7*i);if(!(p[i]&0x80)){*value=v;return i+1;}}
 return 0;
}
static int xzCheckSize(int type){return type==0?0:type<=3?4:type<=6?8:type<=9?16:type<=12?32:64;}

/* output target for a decoded block */
typedef struct { int mode; FILE *out; UInt32 crc; UInt32 total; } XzOut;
static SRes xzEmit(XzOut *o,const Byte *data,size_t n){
 o->crc=CrcUpdate(o->crc,data,n);o->total+=(UInt32)n;
 if(o->mode==2&&o->out&&fwrite(data,1,n,o->out)!=n)return SZ_ERROR_WRITE;
 return SZ_OK;
}

/* Decode one block body (LZMA2) from the current file position until the LZMA2
   end marker. Returns consumed compressed byte count in *consumed. */
static SRes xzDecodeBlock(FILE *f,Byte dictProp,XzOut *o,UInt32 *consumed,ISzAllocPtr alloc){
 CLzma2Dec dec;Byte *inBuf,*outBuf;SRes r;long dataStart=ftell(f);UInt32 used=0;
 Lzma2Dec_Construct(&dec);
 r=Lzma2Dec_Allocate(&dec,dictProp,alloc);if(r!=SZ_OK)return r;
 Lzma2Dec_Init(&dec);
 inBuf=(Byte *)ISzAlloc_Alloc(alloc,XZ_CHUNK);outBuf=(Byte *)ISzAlloc_Alloc(alloc,XZ_CHUNK);
 if(!inBuf||!outBuf){r=SZ_ERROR_MEM;goto done;}
 for(;;){
  size_t got=fread(inBuf,1,XZ_CHUNK,f);size_t pos=0;
  if(got==0){r=SZ_ERROR_INPUT_EOF;break;}
  while(pos<got){
   SizeT inLen=got-pos,outLen=XZ_CHUNK;ELzmaStatus status;
   r=Lzma2Dec_DecodeToBuf(&dec,outBuf,&outLen,inBuf+pos,&inLen,LZMA_FINISH_ANY,&status);
   if(r!=SZ_OK)goto done;
   pos+=inLen;used+=(UInt32)inLen;
   if(outLen){r=xzEmit(o,outBuf,outLen);if(r!=SZ_OK)goto done;}
   if(status==LZMA_STATUS_FINISHED_WITH_MARK){r=SZ_OK;goto finished;}
   if(inLen==0&&outLen==0){r=SZ_ERROR_DATA;goto done;} /* no progress with input present */
  }
  /* inner loop drained this buffer (pos==got); outer loop refills */
 }
finished:
 /* reposition file right after the consumed compressed bytes */
 if(r==SZ_OK&&fseek(f,dataStart+(long)used,SEEK_SET))r=SZ_ERROR_READ;
 if(consumed)*consumed=used;
done:
 Lzma2Dec_Free(&dec,alloc);
 ISzAlloc_Free(alloc,inBuf);ISzAlloc_Free(alloc,outBuf);
 return r;
}

/* mode: 0 list, 1 test, 2 extract */
static SRes xzRun(FILE *f,const char *archivePath,int mode,const char *destArg,ISzAllocPtr alloc){
 Byte sh[12];int checkType,csize;SRes r;FILE *out=NULL;char outpath[300];long pos;int first=1;
 XzOut o;o.mode=mode;o.out=NULL;o.crc=CRC_INIT_VAL;o.total=0;
 if(fseek(f,0,SEEK_SET))return SZ_ERROR_READ;
 if(fread(sh,1,12,f)!=12){strcpy(xzMessage,"ERROR: Not an xz stream.");return SZ_ERROR_NO_ARCHIVE;}
 if(sh[0]!=0xFD||sh[1]!='7'||sh[2]!='z'||sh[3]!='X'||sh[4]!='Z'||sh[5]!=0){strcpy(xzMessage,"ERROR: Not an xz stream.");return SZ_ERROR_NO_ARCHIVE;}
 if(sh[6]!=0){strcpy(xzMessage,"ERROR: Reserved stream flags are set.");return SZ_ERROR_UNSUPPORTED;}
 checkType=sh[7]&0x0F;csize=xzCheckSize(checkType);
 if(mode==2){struct stat st;if(Z7_MKDIR(destArg)&&(stat(destArg,&st)||!(st.st_mode&S_IFDIR))){strcpy(xzMessage,"ERROR: Cannot create the output directory.");return SZ_ERROR_WRITE;}}
 for(;;){
  Byte hdr[1024];int hsize,bcode,bflags,nf,p,fi;UInt32 dictProp=0xFFFFFFFFUL,vli;long blockStart;
  blockStart=ftell(f);if(blockStart<0)return SZ_ERROR_READ;
  if(fread(hdr,1,1,f)!=1){strcpy(xzMessage,"ERROR: The xz stream is truncated.");return SZ_ERROR_DATA;}
  if(hdr[0]==0)break; /* index indicator: blocks done */
  hsize=((int)hdr[0]+1)*4;
  if(hsize>(int)sizeof(hdr)){strcpy(xzMessage,"ERROR: The xz block header is too large.");return SZ_ERROR_UNSUPPORTED;}
  if(fread(hdr+1,1,hsize-1,f)!=(size_t)(hsize-1)){strcpy(xzMessage,"ERROR: The xz stream is truncated.");return SZ_ERROR_DATA;}
  if(CRC_GET_DIGEST(CrcUpdate(CRC_INIT_VAL,hdr,hsize-4))!=((UInt32)hdr[hsize-4]|((UInt32)hdr[hsize-3]<<8)|((UInt32)hdr[hsize-2]<<16)|((UInt32)hdr[hsize-1]<<24))){strcpy(xzMessage,"ERROR: xz block header checksum mismatch.");return SZ_ERROR_CRC;}
  bflags=hdr[1];nf=(bflags&3)+1;p=2;
  if(bflags&0x40){int u=xzVli(hdr+p,hsize-p,&vli);if(!u){r=SZ_ERROR_DATA;goto hdrbad;}p+=u;} /* compressed size present */
  if(bflags&0x80){int u=xzVli(hdr+p,hsize-p,&vli);if(!u){r=SZ_ERROR_DATA;goto hdrbad;}p+=u;} /* uncompressed size present */
  for(fi=0;fi<nf;fi++){
   UInt32 id,ps;int u=xzVli(hdr+p,hsize-p,&id);if(!u){r=SZ_ERROR_DATA;goto hdrbad;}p+=u;
   u=xzVli(hdr+p,hsize-p,&ps);if(!u){r=SZ_ERROR_DATA;goto hdrbad;}p+=u;
   if(id==0x21){if(ps!=1){strcpy(xzMessage,"ERROR: Unexpected LZMA2 property size.");return SZ_ERROR_UNSUPPORTED;}dictProp=hdr[p];}
   else{strcpy(xzMessage,"ERROR: Only the LZMA2 filter is supported in xz blocks.");return SZ_ERROR_UNSUPPORTED;}
   p+=(int)ps;
  }
  if(dictProp==0xFFFFFFFFUL){strcpy(xzMessage,"ERROR: The xz block has no LZMA2 filter.");return SZ_ERROR_UNSUPPORTED;}
  /* open output on the first block for extract */
  if(mode==2&&first){
   char name[260];size_t dl,nl;struct stat st;
   xzDefaultName(archivePath,name,sizeof(name));
   if(!validDosComponent(name,strlen(name))){strcpy(xzMessage,"ERROR: The output name is not a valid DOS file name.");return SZ_ERROR_UNSUPPORTED;}
   dl=strlen(destArg);nl=strlen(name);if(dl+1+nl+1>sizeof(outpath)){strcpy(xzMessage,"ERROR: The output path is too long.");return SZ_ERROR_UNSUPPORTED;}
   memcpy(outpath,destArg,dl);outpath[dl]=Z7_PATH_SEPARATOR;memcpy(outpath+dl+1,name,nl+1);
   if(!stat(outpath,&st)){strcpy(xzMessage,"ERROR: The destination already exists. No output was created.");return SZ_ERROR_PARAM;}
   out=fopen(outpath,"wb");if(!out){strcpy(xzMessage,"ERROR: Cannot create the output file.");return SZ_ERROR_WRITE;}
   o.out=out;
  }
  first=0;
  /* seek to block data (blockStart + hsize) and decode */
  if(fseek(f,blockStart+hsize,SEEK_SET)){r=SZ_ERROR_READ;goto failed;}
  r=xzDecodeBlock(f,(Byte)dictProp,&o,NULL,alloc);
  if(r!=SZ_OK)goto failed;
  /* skip block padding to 4-byte boundary, then the check */
  pos=ftell(f);if(pos<0){r=SZ_ERROR_READ;goto failed;}
  if(pos&3){if(fseek(f,4-(pos&3),SEEK_CUR)){r=SZ_ERROR_READ;goto failed;}}
  if(checkType==1){ /* CRC32 */
   Byte c[4];if(fread(c,1,4,f)!=4){r=SZ_ERROR_DATA;goto failed;}
   if(CRC_GET_DIGEST(o.crc)!=((UInt32)c[0]|((UInt32)c[1]<<8)|((UInt32)c[2]<<16)|((UInt32)c[3]<<24))){strcpy(xzMessage,"ERROR: xz CRC32 check mismatch.");r=SZ_ERROR_CRC;goto failed;}
  }else if(csize){if(fseek(f,csize,SEEK_CUR)){r=SZ_ERROR_READ;goto failed;}}
  o.crc=CRC_INIT_VAL; /* per-block CRC reset (each block has its own check) */
 }
 if(mode==2&&out){if(fclose(out)){strcpy(xzMessage,"ERROR: Could not finish the output file.");return SZ_ERROR_WRITE;}out=NULL;}
 (void)archivePath;
 return SZ_OK;
hdrbad:
 strcpy(xzMessage,"ERROR: The xz block header is malformed.");
 return r;
failed:
 if(out){fclose(out);remove(outpath);}
 if(!xzMessage[0])strcpy(xzMessage,"ERROR: The xz stream is damaged.");
 return r;
}
/* Read a VLI directly from the file (base-128 LE). Returns bytes used or 0. */
static int xzReadVliF(FILE *f,UInt32 *v){UInt32 x=0;int i;for(i=0;i<9;i++){int c=fgetc(f);if(c==EOF)return 0;x|=(UInt32)(c&0x7F)<<(7*i);if(!(c&0x80)){*v=x;return i+1;}}return 0;}
/* List reads the uncompressed size from the xz index (no decode required). */
static SRes xzList(FILE *f,const char *archivePath){
 Byte sh[12],foot[12];long end,indexPos;UInt32 backSize,indexSize,count,i,total=0;char name[260];
 xzMessage[0]=0;
 if(fseek(f,0,SEEK_SET)||fread(sh,1,12,f)!=12||sh[0]!=0xFD||sh[1]!='7'||sh[2]!='z'||sh[3]!='X'||sh[4]!='Z'||sh[5]!=0){strcpy(xzMessage,"ERROR: Not an xz stream.");return SZ_ERROR_NO_ARCHIVE;}
 if(fseek(f,0,SEEK_END))return SZ_ERROR_READ;end=ftell(f);if(end<24){strcpy(xzMessage,"ERROR: The xz stream is truncated.");return SZ_ERROR_DATA;}
 if(fseek(f,end-12,SEEK_SET)||fread(foot,1,12,f)!=12)return SZ_ERROR_READ;
 if(foot[10]!='Y'||foot[11]!='Z'){strcpy(xzMessage,"ERROR: Bad xz footer.");return SZ_ERROR_DATA;}
 backSize=(UInt32)foot[4]|((UInt32)foot[5]<<8)|((UInt32)foot[6]<<16)|((UInt32)foot[7]<<24);
 indexSize=(backSize+1)*4;if((long)indexSize>end-12){strcpy(xzMessage,"ERROR: Bad xz index size.");return SZ_ERROR_DATA;}
 indexPos=end-12-(long)indexSize;
 if(fseek(f,indexPos,SEEK_SET))return SZ_ERROR_READ;
 if(fgetc(f)!=0){strcpy(xzMessage,"ERROR: Bad xz index indicator.");return SZ_ERROR_DATA;}
 if(!xzReadVliF(f,&count)){strcpy(xzMessage,"ERROR: Bad xz index.");return SZ_ERROR_DATA;}
 for(i=0;i<count;i++){UInt32 unpadded,usize;if(!xzReadVliF(f,&unpadded)||!xzReadVliF(f,&usize)){strcpy(xzMessage,"ERROR: Bad xz index record.");return SZ_ERROR_DATA;}total+=usize;}
 xzDefaultName(archivePath,name,sizeof(name));
 printf("F %lu %s\n",(unsigned long)total,name);
 return SZ_OK;
}
static SRes xzTest(FILE *f,ISzAllocPtr alloc){xzMessage[0]=0;return xzRun(f,"x.xz",1,NULL,alloc);}
static SRes xzExtract(FILE *f,const char *archivePath,const char *destDir,ISzAllocPtr alloc){xzMessage[0]=0;return xzRun(f,archivePath,2,destDir,alloc);}

#ifdef Z7_WIN16_WRITE_XZ_AES
/* ---- xz create (single file) over LZMA2 in independent 8 KiB-dictionary
   chunks (dict reset per chunk), check = CRC32. Reuses the shared LZMA
   encoder. One .xz holds exactly one file, like gzip.
   Gated behind Z7_WIN16_WRITE_XZ_AES: compiling this into the real-mode CLI
   shifts memory enough to tip the memory-marginal LZMA encoder into
   pathological slowness, so the default build omits it. ---- */
#define XZ_ENC_CHUNK 8192
#define XZ_ENC_DICTPROP 2          /* LZMA2 dict size code for 8192 */
static UInt32 xzCrc32(const Byte *p,size_t n){return CRC_GET_DIGEST(CrcUpdate(CRC_INIT_VAL,p,n));}
static int xzVliBuf(Byte *b,int *ip,UInt32 v){do{Byte x=(Byte)(v&0x7f);v>>=7;if(v)x|=0x80;b[(*ip)++]=x;}while(v);return 1;}
static SRes xzCreate(const char *path,const char *source,ISzAllocPtr alloc){
 FILE *in,*out;struct stat st;SRes r=SZ_OK;
 CLzmaEncHandle enc=NULL;CLzmaEncProps props;Byte lzprops[5];SizeT lpsz=5;
 Byte *src=NULL,*dst=NULL;UInt32 uncCrc=CRC_INIT_VAL,totalUnc=0,lzma2Len=0;
 Byte sh[12],bh[12];unsigned pad;
 xzMessage[0]=0;
 if(!stat(path,&st)){strcpy(xzMessage,"ERROR: Existing archives are never overwritten.");return SZ_ERROR_PARAM;}
 if(stat(source,&st)||(st.st_mode&S_IFDIR)){strcpy(xzMessage,"ERROR: xz stores exactly one regular file.");return SZ_ERROR_PARAM;}
 in=fopen(source,"rb");if(!in){strcpy(xzMessage,"ERROR: Cannot open the input file.");return SZ_ERROR_READ;}
 out=fopen(path,"wb");if(!out){fclose(in);strcpy(xzMessage,"ERROR: Cannot create the archive.");return SZ_ERROR_WRITE;}
 src=(Byte *)ISzAlloc_Alloc(alloc,XZ_ENC_CHUNK);
 dst=(Byte *)ISzAlloc_Alloc(alloc,XZ_ENC_CHUNK+XZ_ENC_CHUNK/8+512);
 enc=LzmaEnc_Create(alloc);
 if(!src||!dst||!enc){r=SZ_ERROR_MEM;goto done;}
 LzmaEncProps_Init(&props);props.dictSize=XZ_ENC_CHUNK;props.numThreads=1;props.algo=0;
 r=LzmaEnc_SetProps(enc,&props);if(r!=SZ_OK)goto done;
 r=LzmaEnc_WriteProperties(enc,lzprops,&lpsz);if(r!=SZ_OK)goto done;
 /* stream header: magic, flags(00, check=CRC32=01), CRC32 of the 2 flag bytes */
 sh[0]=0xFD;sh[1]='7';sh[2]='z';sh[3]='X';sh[4]='Z';sh[5]=0;sh[6]=0x00;sh[7]=0x01;
 {UInt32 c=xzCrc32(sh+6,2);sh[8]=(Byte)c;sh[9]=(Byte)(c>>8);sh[10]=(Byte)(c>>16);sh[11]=(Byte)(c>>24);}
 if(fwrite(sh,1,12,out)!=12){r=SZ_ERROR_WRITE;goto done;}
 /* block header: size byte, flags(1 filter), LZMA2 filter id+propsize+dict, pad, CRC32 */
 bh[0]=0x02;bh[1]=0x00;bh[2]=0x21;bh[3]=0x01;bh[4]=XZ_ENC_DICTPROP;bh[5]=0;bh[6]=0;bh[7]=0;
 {UInt32 c=xzCrc32(bh,8);bh[8]=(Byte)c;bh[9]=(Byte)(c>>8);bh[10]=(Byte)(c>>16);bh[11]=(Byte)(c>>24);}
 if(fwrite(bh,1,12,out)!=12){r=SZ_ERROR_WRITE;goto done;}
 /* LZMA2 chunks */
 for(;;){
  size_t got=fread(src,1,XZ_ENC_CHUNK,in);SizeT destLen;Byte ctrl;
  if(got==0){if(ferror(in))r=SZ_ERROR_READ;break;}
  destLen=XZ_ENC_CHUNK+XZ_ENC_CHUNK/8+512;
  r=LzmaEnc_MemEncode(enc,dst,&destLen,src,got,0,NULL,alloc,alloc);
  if(r!=SZ_OK)goto done;
  if(destLen>65536||destLen==0){r=SZ_ERROR_FAIL;goto done;}
  uncCrc=CrcUpdate(uncCrc,src,got);totalUnc+=(UInt32)got;
  ctrl=(Byte)(0x80|(3<<5)|(((UInt32)got-1)>>16));
  /* LZMA2 chunk sizes are stored big-endian (high byte first). */
  if(fputc(ctrl,out)==EOF||fputc((int)((((UInt32)got-1)>>8)&0xff),out)==EOF||fputc((int)(((UInt32)got-1)&0xff),out)==EOF
     ||fputc((int)(((destLen-1)>>8)&0xff),out)==EOF||fputc((int)((destLen-1)&0xff),out)==EOF
     ||fputc(lzprops[0],out)==EOF||fwrite(dst,1,destLen,out)!=destLen){r=SZ_ERROR_WRITE;goto done;}
  lzma2Len+=6+(UInt32)destLen;
  if(got<XZ_ENC_CHUNK)break;
 }
 if(r!=SZ_OK)goto done;
 if(fputc(0x00,out)==EOF){r=SZ_ERROR_WRITE;goto done;}lzma2Len+=1;   /* LZMA2 end marker */
 pad=(unsigned)((4-(lzma2Len&3))&3);while(pad--)if(fputc(0,out)==EOF){r=SZ_ERROR_WRITE;goto done;}
 {UInt32 c;Byte cb[4];c=CRC_GET_DIGEST(uncCrc);cb[0]=(Byte)c;cb[1]=(Byte)(c>>8);cb[2]=(Byte)(c>>16);cb[3]=(Byte)(c>>24);if(fwrite(cb,1,4,out)!=4){r=SZ_ERROR_WRITE;goto done;}}
 /* index */
 {Byte ib[64];int ip=0;UInt32 c,unpadded=12+lzma2Len+4;unsigned ipad;
  ib[ip++]=0x00;xzVliBuf(ib,&ip,1);xzVliBuf(ib,&ip,unpadded);xzVliBuf(ib,&ip,totalUnc);
  ipad=(unsigned)((4-(ip&3))&3);while(ipad--)ib[ip++]=0;
  c=xzCrc32(ib,ip);ib[ip++]=(Byte)c;ib[ip++]=(Byte)(c>>8);ib[ip++]=(Byte)(c>>16);ib[ip++]=(Byte)(c>>24);
  if(fwrite(ib,1,ip,out)!=(size_t)ip){r=SZ_ERROR_WRITE;goto done;}
  /* footer: CRC32(backward+flags), backward size, stream flags, "YZ" */
  {Byte ft[12];UInt32 back=(UInt32)(ip/4-1),fc;
   ft[4]=(Byte)back;ft[5]=(Byte)(back>>8);ft[6]=(Byte)(back>>16);ft[7]=(Byte)(back>>24);
   ft[8]=0x00;ft[9]=0x01;fc=xzCrc32(ft+4,6);ft[0]=(Byte)fc;ft[1]=(Byte)(fc>>8);ft[2]=(Byte)(fc>>16);ft[3]=(Byte)(fc>>24);
   ft[10]='Y';ft[11]='Z';if(fwrite(ft,1,12,out)!=12){r=SZ_ERROR_WRITE;goto done;}}
 }
done:
 if(enc)LzmaEnc_Destroy(enc,alloc,alloc);
 ISzAlloc_Free(alloc,src);ISzAlloc_Free(alloc,dst);
 fclose(in);if(fclose(out)&&r==SZ_OK)r=SZ_ERROR_WRITE;
 if(r!=SZ_OK){remove(path);if(!xzMessage[0])strcpy(xzMessage,"ERROR: Could not write the xz archive.");}
 return r;
}
#endif /* Z7_WIN16_WRITE_XZ_AES */
#endif
