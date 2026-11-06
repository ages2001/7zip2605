/* POSIX ustar (tar) container, store-only (tar performs no compression).
   Create walks the shared input tree; extract/list/test parse 512-byte header
   blocks. Names use '/'; DOS 8.3 components are validated on extraction.
   Shared by the CLI and the GUI front ends.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_TAR_FORMAT_H
#define Z7_WIN16_TAR_FORMAT_H
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../../C/7zTypes.h"
#include "dos_paths.h"
#include "input_tree.h"

static char tarMessage[256];
#define TAR_BLOCK 512

/* ---- octal helpers ---- */
static void tarOctal(char *field,size_t width,UInt32 value){
 /* width-1 octal digits, right-justified, zero-filled, then a NUL. */
 size_t i;for(i=0;i+1<width;i++){field[width-2-i]=(char)('0'+(value&7));value>>=3;}field[width-1]=0;
}
static UInt32 tarParseOctal(const char *field,size_t width){
 UInt32 v=0;size_t i=0;while(i<width&&(field[i]==' '||field[i]==0))i++;
 for(;i<width&&field[i]>='0'&&field[i]<='7';i++)v=(v<<3)+(UInt32)(field[i]-'0');
 return v;
}
static UInt32 tarChecksum(const Byte *block){
 UInt32 sum=0;int i;for(i=0;i<TAR_BLOCK;i++)sum+=(i>=148&&i<156)?(UInt32)' ':(UInt32)block[i];return sum;
}

/* ---- name helpers ---- */
static int tarIsName(const char *path){size_t n=strlen(path);return n>4&&sameName(path+n-4,".TAR");}
/* Copy an archive name, mapping '\' to '/'; append a slash for directories. */
static int tarBuildStoredName(char *out,size_t max,const char *name,int directory){
 size_t n=strlen(name),i;if(n+(directory?1:0)+1>max||n+(directory?1:0)>100)return 0;
 for(i=0;i<n;i++)out[i]=(name[i]=='\\')?'/':name[i];
 if(directory)out[n++]='/';out[n]=0;return 1;
}

/* ---- create ---- */
static SRes tarWriteHeader(FILE *out,const char *storedName,UInt32 size,UInt32 mtime,int directory){
 Byte h[TAR_BLOCK];char num[12];UInt32 sum;
 memset(h,0,sizeof(h));
 strncpy((char *)h,storedName,100);
 tarOctal((char *)h+100,8,directory?0755UL:0644UL);   /* mode  */
 tarOctal((char *)h+108,8,0);                          /* uid   */
 tarOctal((char *)h+116,8,0);                          /* gid   */
 tarOctal(num,12,size);memcpy(h+124,num,12);           /* size  */
 tarOctal(num,12,mtime);memcpy(h+136,num,12);          /* mtime */
 memset(h+148,' ',8);                                  /* chksum field = spaces */
 h[156]=directory?'5':'0';                             /* typeflag */
 memcpy(h+257,"ustar",5);h[262]=0;h[263]='0';h[264]='0'; /* magic+version */
 sum=tarChecksum(h);
 {char c[8];tarOctal(c,7,sum);c[7]=' ';memcpy(h+148,c,8);} /* 6 octal + NUL + space */
 return fwrite(h,1,TAR_BLOCK,out)==TAR_BLOCK?SZ_OK:SZ_ERROR_WRITE;
}
static SRes tarCopyData(FILE *out,const char *source,UInt32 size){
 FILE *in;Byte block[TAR_BLOCK];UInt32 left=size;size_t got;
 in=fopen(source,"rb");if(!in)return SZ_ERROR_READ;
 while(left){
  size_t want=left<TAR_BLOCK?(size_t)left:TAR_BLOCK;
  got=fread(block,1,want,in);
  if(got!=want){fclose(in);return SZ_ERROR_READ;}
  if(got<TAR_BLOCK)memset(block+got,0,TAR_BLOCK-got);
  if(fwrite(block,1,TAR_BLOCK,out)!=TAR_BLOCK){fclose(in);return SZ_ERROR_WRITE;}
  left-=(UInt32)want;
 }
 return fclose(in)?SZ_ERROR_READ:SZ_OK;
}
static SRes tarCreate(const char *path,unsigned count,const char *const *sources,ISzAllocPtr alloc){
 InputTree tree;FILE *out;SRes r;unsigned i;Byte zero[TAR_BLOCK];struct stat st;
 tarMessage[0]=0;
 if(!stat(path,&st)){strcpy(tarMessage,"ERROR: Existing archives are never overwritten.");return SZ_ERROR_PARAM;}
 r=inputTreeBuild(&tree,count,sources,NULL,alloc);
 if(r!=SZ_OK){if(inputTreeMessage[0])strcpy(tarMessage,inputTreeMessage);inputTreeFree(&tree,alloc);return r;}
 out=fopen(path,"wb");if(!out){inputTreeFree(&tree,alloc);strcpy(tarMessage,"ERROR: Cannot create the archive.");return SZ_ERROR_WRITE;}
 for(i=0;r==SZ_OK&&i<tree.count;i++){
  char stored[106];UInt32 size=0,mtime=0;
  if(!tree.items[i].directory){if(stat(tree.items[i].source,&st)){r=SZ_ERROR_READ;break;}size=(UInt32)st.st_size;mtime=(UInt32)st.st_mtime;}
  else{if(!stat(tree.items[i].source,&st))mtime=(UInt32)st.st_mtime;}
  if(!tarBuildStoredName(stored,sizeof(stored),tree.items[i].name,tree.items[i].directory)){strcpy(tarMessage,"ERROR: A stored name is too long for ustar (100 characters).");r=SZ_ERROR_UNSUPPORTED;break;}
  r=tarWriteHeader(out,stored,size,mtime,tree.items[i].directory);
  if(r==SZ_OK&&!tree.items[i].directory)r=tarCopyData(out,tree.items[i].source,size);
 }
 if(r==SZ_OK){memset(zero,0,sizeof(zero));if(fwrite(zero,1,TAR_BLOCK,out)!=TAR_BLOCK||fwrite(zero,1,TAR_BLOCK,out)!=TAR_BLOCK)r=SZ_ERROR_WRITE;}
 inputTreeFree(&tree,alloc);
 if(fclose(out)&&r==SZ_OK)r=SZ_ERROR_WRITE;
 if(r!=SZ_OK){remove(path);if(!tarMessage[0])strcpy(tarMessage,"ERROR: Could not write the tar archive.");}
 return r;
}

/* ---- read side: walk blocks ---- */
/* mode: 0 = list, 1 = test, 2 = extract into destDir. */
static int tarBlockIsZero(const Byte *b){int i;for(i=0;i<TAR_BLOCK;i++)if(b[i])return 0;return 1;}
static SRes tarWalk(FILE *f,int mode,const char *destDir,int flat,ISzAllocPtr alloc){
 Byte h[TAR_BLOCK],data[TAR_BLOCK];int zeros=0;SRes r=SZ_OK;(void)alloc;
 if(fseek(f,0,SEEK_SET))return SZ_ERROR_READ;
 if(mode==2){struct stat st;if(Z7_MKDIR(destDir)&&(stat(destDir,&st)||!(st.st_mode&S_IFDIR))){strcpy(tarMessage,"ERROR: Cannot create the output directory.");return SZ_ERROR_WRITE;}}
 for(;;){
  size_t got=fread(h,1,TAR_BLOCK,f);
  if(got==0)break;
  if(got!=TAR_BLOCK){strcpy(tarMessage,"ERROR: The tar archive is truncated.");return SZ_ERROR_DATA;}
  if(tarBlockIsZero(h)){if(++zeros>=2)break;continue;}
  zeros=0;
  if(memcmp(h+257,"ustar",5)){strcpy(tarMessage,"ERROR: Not a ustar header.");return SZ_ERROR_DATA;}
  if(tarParseOctal((char *)h+148,8)!=tarChecksum(h)){strcpy(tarMessage,"ERROR: Header checksum mismatch.");return SZ_ERROR_CRC;}
  {
   char name[270];UInt32 size=tarParseOctal((char *)h+124,12);int type=h[156];size_t pl=0,nl;UInt32 blocks,b;int isdir;
   /* prefix + '/' + name */
   if(h[345]){for(pl=0;pl<155&&h[345+pl];pl++)name[pl]=(char)h[345+pl];name[pl++]='/';}
   for(nl=0;nl<100&&h[nl];nl++)name[pl+nl]=(char)h[nl];name[pl+nl]=0;
   nl=strlen(name);isdir=(type=='5')||(nl&&name[nl-1]=='/');
   if(flat&&!isdir){size_t j,base=0;for(j=0;j<nl;j++)if(name[j]=='/')base=j+1;if(base){memmove(name,name+base,nl-base+1);nl-=base;}}
   if(mode==0){printf("%c %lu %s\n",isdir?'D':'F',(unsigned long)size,name);}
   /* data blocks follow for regular files */
   blocks=(size+TAR_BLOCK-1)/TAR_BLOCK;
   if(type!='0'&&type!=0&&type!='5'){ /* unsupported entry: skip its data */
    for(b=0;b<blocks;b++){if(fread(data,1,TAR_BLOCK,f)!=TAR_BLOCK){strcpy(tarMessage,"ERROR: The tar archive is truncated.");return SZ_ERROR_DATA;}}
    continue;
   }
   if(mode==2&&!isdir){
    /* build + validate DOS path under destDir */
    char path[300];size_t dl=strlen(destDir),i2,comp;FILE *o;struct stat st;int bad=0;
    if(dl+1+nl+1>sizeof(path)){strcpy(tarMessage,"ERROR: An output path is too long.");return SZ_ERROR_UNSUPPORTED;}
    memcpy(path,destDir,dl);path[dl]=Z7_PATH_SEPARATOR;
    comp=dl+1;for(i2=0;i2<nl;i2++){char c=name[i2];if(c=='/'){if(!validDosComponent(path+comp,(unsigned)(dl+1+i2-comp))){bad=1;break;}path[dl+1+i2]=Z7_PATH_SEPARATOR;comp=dl+1+i2+1;}else path[dl+1+i2]=c;}
    path[dl+1+nl]=0;
    if(bad||!validDosComponent(path+comp,(unsigned)(strlen(path)-comp))){strcpy(tarMessage,"ERROR: A stored name is not a valid DOS path.");return SZ_ERROR_UNSUPPORTED;}
    /* create parent directories */
    for(i2=dl+1;path[i2];i2++)if(path[i2]==Z7_PATH_SEPARATOR){path[i2]=0;if(Z7_MKDIR(path)&&(stat(path,&st)||!(st.st_mode&S_IFDIR))){strcpy(tarMessage,"ERROR: Cannot create an output directory.");return SZ_ERROR_WRITE;}path[i2]=Z7_PATH_SEPARATOR;}
    if(!stat(path,&st)){strcpy(tarMessage,"ERROR: A destination already exists. No conflicting file was overwritten.");return SZ_ERROR_PARAM;}
    o=fopen(path,"wb");if(!o){strcpy(tarMessage,"ERROR: Cannot create an output file.");return SZ_ERROR_WRITE;}
    {UInt32 left=size;for(b=0;b<blocks;b++){size_t take;if(fread(data,1,TAR_BLOCK,f)!=TAR_BLOCK){fclose(o);remove(path);strcpy(tarMessage,"ERROR: The tar archive is truncated.");return SZ_ERROR_DATA;}take=left<TAR_BLOCK?(size_t)left:TAR_BLOCK;if(take&&fwrite(data,1,take,o)!=take){fclose(o);remove(path);return SZ_ERROR_WRITE;}left-=(UInt32)take;}}
    if(fclose(o)){remove(path);return SZ_ERROR_WRITE;}
   }else if(mode==2&&isdir&&flat){
    /* flat extraction drops directory entries */
   }else if(mode==2&&isdir){
    char path[300];size_t dl=strlen(destDir),i2,comp;struct stat st;int bad=0;size_t cn=nl;while(cn&&name[cn-1]=='/')cn--;
    if(dl+1+cn+1>sizeof(path)){strcpy(tarMessage,"ERROR: An output path is too long.");return SZ_ERROR_UNSUPPORTED;}
    memcpy(path,destDir,dl);path[dl]=Z7_PATH_SEPARATOR;comp=dl+1;
    for(i2=0;i2<cn;i2++){char c=name[i2];if(c=='/'){if(!validDosComponent(path+comp,(unsigned)(dl+1+i2-comp))){bad=1;break;}path[dl+1+i2]=Z7_PATH_SEPARATOR;comp=dl+1+i2+1;}else path[dl+1+i2]=c;}
    path[dl+1+cn]=0;
    if(!bad&&validDosComponent(path+comp,(unsigned)(strlen(path)-comp))){for(i2=dl+1;path[i2];i2++)if(path[i2]==Z7_PATH_SEPARATOR){path[i2]=0;Z7_MKDIR(path);path[i2]=Z7_PATH_SEPARATOR;}Z7_MKDIR(path);}
   }else{ /* list/test: consume data */
    for(b=0;b<blocks;b++){if(fread(data,1,TAR_BLOCK,f)!=TAR_BLOCK){strcpy(tarMessage,"ERROR: The tar archive is truncated.");return SZ_ERROR_DATA;}}
   }
  }
 }
 return r;
}
static SRes tarList(FILE *f){tarMessage[0]=0;return tarWalk(f,0,NULL,0,NULL);}
static SRes tarTest(FILE *f,ISzAllocPtr alloc){tarMessage[0]=0;return tarWalk(f,1,NULL,0,alloc);}
static SRes tarExtract(FILE *f,const char *destDir,int flat,ISzAllocPtr alloc){tarMessage[0]=0;return tarWalk(f,2,destDir,flat,alloc);}
static int tarMagicFile(FILE *f){Byte m[8];if(fseek(f,257,SEEK_SET))return 0;if(fread(m,1,5,f)!=5){fseek(f,0,SEEK_SET);return 0;}fseek(f,0,SEEK_SET);return !memcmp(m,"ustar",5);}
#endif
