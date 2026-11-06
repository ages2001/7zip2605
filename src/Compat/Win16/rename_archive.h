/* Lossless 7z name updates. Preserve opaque properties and every packed byte.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_RENAME_ARCHIVE_H
#define Z7_WIN16_RENAME_ARCHIVE_H
#ifdef __WATCOMC__
#include <share.h>
#endif
static char renameMessage[320];
static unsigned renameChar(const Byte *p){unsigned ch=p[0]|((unsigned)p[1]<<8);return ch=='\\'?'/':ch;}
static unsigned renameFold(unsigned ch){return ch>='a'&&ch<='z'?ch-'a'+'A':ch;}
static int renameMatch(const Byte *name,const char *text,int subtree){
 unsigned i;for(i=0;text[i];i++){unsigned ch=(unsigned char)text[i];if(ch=='\\')ch='/';if(renameChar(name+2*i)!=ch)return 0;}
 return !renameChar(name+2*i)||(subtree&&renameChar(name+2*i)=='/');
}
static int renamePath(const char *path){
 const char *start=path,*p;unsigned length=(unsigned)strlen(path);
 if(!length||length>=260||*path=='/'||*path=='\\')return 0;
 for(p=path;;p++)if(*p=='/'||*p=='\\'||!*p){
  if(!validDosComponent(start,(unsigned)(p-start)))return 0;
  if(!*p)break;start=p+1;
 }
 return 1;
}
static int renameCollision(const Byte *a,const Byte *b,int aDir,int bDir){
 unsigned i=0,ca,cb;
 for(;;){ca=renameFold(renameChar(a+2*i));cb=renameFold(renameChar(b+2*i));if(ca!=cb)break;if(!ca)return 1;i++;}
 return (!ca&&cb=='/'&&!aDir)||(!cb&&ca=='/'&&!bDir);
}
/* Each generated sibling uses DOS 8.3 syntax. Creation is exclusive; existing
   files are never used as temporary output or as a backup. */
static int renameSibling(const char *archive,char *path,unsigned attempt,const char *ext){
 const char *p,*base=archive;size_t prefix;
 for(p=archive;*p;p++)if(*p=='/'||*p=='\\'||*p==':')base=p+1;
 prefix=(size_t)(base-archive);if(prefix+13>=260)return 0;
 memcpy(path,archive,prefix);sprintf(path+prefix,"7R%06X.%s",attempt,ext);return 1;
}
static int renameBackup(const char *source,const char *backup){
#ifdef __WATCOMC__
 /* DOS rename fails if the destination exists. */
 return rename(source,backup);
#else
 /* POSIX rename can replace its destination; use an exclusive hard link for
    the backup instead. The source remains intact until the atomic commit. */
 return link(source,backup);
#endif
}
static SRes renameArchive(const char *path,const char *oldName,const char *newName,ISzAllocPtr allocator){
 Input in;CLookToRead2 look;CSzArEx a,verify;CreateHeader header,start;
 Byte *lookBuf=NULL,*copyBuf=NULL,signature[32];size_t *offsets=NULL;
 FILE *out=NULL;unsigned i,j,oldIndex=0,changed=0,attempt;int found=0,subtree=0,owned=0,backed=0,committed=0,fd;
 UInt64 newNames=0,processed=0,fileSize;UInt32 sourceCRC=CRC_INIT_VAL,checkCRC=CRC_INIT_VAL;
 SRes result=SZ_OK;struct stat original,current;char temporary[260],backup[260];unsigned attrs=0;
 size_t prefix,suffix,oldLength,newLength,namesStart;long length;
 renameMessage[0]=0;temporary[0]=backup[0]=0;in.file=NULL;memset(&header,0,sizeof(header));SzArEx_Init(&a);SzArEx_Init(&verify);
 if(!renamePath(oldName)||!renamePath(newName)){strcpy(renameMessage,"ERROR: Rename paths must use relative DOS 8.3 components.");return SZ_ERROR_UNSUPPORTED;}
#ifdef __WATCOMC__
 if(!validDosDestination(path)){strcpy(renameMessage,"ERROR: Archive path must use DOS 8.3 components.");return SZ_ERROR_UNSUPPORTED;}
 if(_dos_getfileattr(path,&attrs)||(attrs&1)){strcpy(renameMessage,"ERROR: Cannot update a read-only or inaccessible archive.");return SZ_ERROR_WRITE;}
 in.file=_fsopen(path,"rb",SH_DENYWR);
#else
 in.file=fopen(path,"rb");
#endif
 if(!in.file||stat(path,&original)){result=SZ_ERROR_READ;goto done;}
 if(!(original.st_mode&S_IWUSR)){result=SZ_ERROR_WRITE;goto done;}
 lookBuf=(Byte *)ISzAlloc_Alloc(allocator,4096);copyBuf=(Byte *)ISzAlloc_Alloc(allocator,4096);
 if(!lookBuf||!copyBuf){result=SZ_ERROR_MEM;goto done;}
 in.iface.Read=readInput;in.iface.Seek=seekInput;in.extraMetadata=0;
 LookToRead2_CreateVTable(&look,False);look.realStream=&in.iface;look.buf=lookBuf;look.bufSize=4096;LookToRead2_INIT(&look);
 a.SaveHeader=True;result=SzArEx_Open(&a,&look.vt,allocator,allocator);if(result!=SZ_OK)goto done;
 if(a.NumFiles>4096){result=SZ_ERROR_MEM;goto done;}
 if(!a.SavedHeader||!a.NamePropertySize){result=SZ_ERROR_UNSUPPORTED;goto done;}
 oldLength=strlen(oldName);newLength=strlen(newName);
 for(i=0;i<a.NumFiles;i++)if(renameMatch(a.FileNames+2*a.FileNameOffsets[i],oldName,0)){if(found){result=SZ_ERROR_ARCHIVE;goto done;}found=1;oldIndex=i;}
 if(!found){strcpy(renameMessage,"ERROR: The archive entry was not found.");result=SZ_ERROR_PARAM;goto done;}
 if(!strcmp(oldName,newName)){strcpy(renameMessage,"The archive name is unchanged.");goto done;}
 subtree=SzArEx_IsDir(&a,oldIndex);
 for(i=0;i<a.NumFiles;i++){
  size_t bytes=2*(a.FileNameOffsets[i+1]-a.FileNameOffsets[i]);
  if(renameMatch(a.FileNames+2*a.FileNameOffsets[i],oldName,subtree)){
   newNames+=(UInt64)bytes-2*oldLength+2*newLength;changed++;
  }else newNames+=bytes;
 }
 prefix=a.NamePropertyOffset;suffix=prefix+a.NamePropertySize;
 if(suffix>a.SavedHeaderSize||newNames>32700||a.SavedHeaderSize-a.NamePropertySize+newNames+11>CREATE_HEADER_CAPACITY){result=SZ_ERROR_MEM;goto done;}
 header.data=(Byte *)ISzAlloc_Alloc(allocator,CREATE_HEADER_CAPACITY);
 if(a.NumFiles>8190){result=SZ_ERROR_MEM;goto done;}
 offsets=(size_t *)ISzAlloc_Alloc(allocator,(size_t)(a.NumFiles+1)*sizeof(size_t));
 if(!header.data||!offsets){result=SZ_ERROR_MEM;goto done;}
 memcpy(header.data,a.SavedHeader,prefix);header.used=(unsigned)prefix;
 createByte(&header,17);createUInt(&header,newNames+1);createByte(&header,0);namesStart=header.used;
 for(i=0;i<a.NumFiles;i++){
  const Byte *name=a.FileNames+2*a.FileNameOffsets[i];size_t bytes=2*(a.FileNameOffsets[i+1]-a.FileNameOffsets[i]);
  offsets[i]=header.used-namesStart;
  if(renameMatch(name,oldName,subtree)){
   for(j=0;j<newLength;j++)createLE(&header,newName[j]=='\\'?'/':(unsigned char)newName[j],2);
   name+=2*oldLength;bytes-=2*oldLength;
  }
  while(bytes--)createByte(&header,*name++);
 }
 offsets[a.NumFiles]=header.used-namesStart;
 for(i=0;i<a.NumFiles;i++)for(j=0;j<i;j++)if(renameCollision(header.data+namesStart+offsets[i],header.data+namesStart+offsets[j],SzArEx_IsDir(&a,i),SzArEx_IsDir(&a,j))){
  strcpy(renameMessage,"ERROR: The new name would cause a file or directory conflict.");result=SZ_ERROR_UNSUPPORTED;goto done;
 }
 for(i=(unsigned)suffix;i<a.SavedHeaderSize;i++)createByte(&header,a.SavedHeader[i]);
 if(header.failed){result=SZ_ERROR_MEM;goto done;}
 if(fseek(in.file,0,SEEK_END)||(length=ftell(in.file))<32){result=SZ_ERROR_READ;goto done;}fileSize=(UInt64)length;
 if(fileSize+header.used>0x7fffffffUL){result=SZ_ERROR_UNSUPPORTED;goto done;}
 if(fseek(in.file,0,SEEK_SET)||fread(signature,1,32,in.file)!=32){result=SZ_ERROR_READ;goto done;}
 if(testProgress&&!testProgress(0)){result=SZ_ERROR_PROGRESS;goto done;}
 for(attempt=0;attempt<4096;attempt++){
  if(!renameSibling(path,temporary,attempt,"TMP")){result=SZ_ERROR_UNSUPPORTED;goto done;}
  fd=open(temporary,O_RDWR|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);if(fd>=0)break;
 }
 if(attempt==4096){result=SZ_ERROR_WRITE;goto done;}owned=1;out=fdopen(fd,"w+b");if(!out){close(fd);result=SZ_ERROR_WRITE;goto done;}
 rewind(in.file);
 while(processed<fileSize){size_t n=fileSize-processed>4096?4096:(size_t)(fileSize-processed);
  if(testProgress&&!testProgress(processed)){result=SZ_ERROR_PROGRESS;goto done;}
  if(fread(copyBuf,1,n,in.file)!=n){result=SZ_ERROR_READ;goto done;}
  sourceCRC=CrcUpdate(sourceCRC,copyBuf,n);if(!createWriteBytes(out,copyBuf,n)){result=SZ_ERROR_WRITE;goto done;}processed+=n;
 }
 if(fgetc(in.file)!=EOF||ferror(in.file)){result=SZ_ERROR_READ;goto done;}
 if(!createWriteBytes(out,header.data,header.used)){result=SZ_ERROR_WRITE;goto done;}
 start.data=signature;start.used=12;start.failed=0;createLE(&start,fileSize-32,8);createLE(&start,header.used,8);createLE(&start,CrcCalc(header.data,header.used),4);
 start.used=8;createLE(&start,CrcCalc(signature+12,20),4);
 if(fseek(out,0,SEEK_SET)||!createWriteBytes(out,signature,32)||fflush(out)){result=SZ_ERROR_WRITE;goto done;}
 if(fclose(out)){out=NULL;result=SZ_ERROR_WRITE;goto done;}out=NULL;
 if(fclose(in.file)){in.file=NULL;result=SZ_ERROR_READ;goto done;}in.file=NULL;
 /* Reopen and validate the candidate before touching the original path. */
 in.file=fopen(temporary,"rb");if(!in.file){result=SZ_ERROR_READ;goto done;}
 LookToRead2_INIT(&look);result=SzArEx_Open(&verify,&look.vt,allocator,allocator);if(result!=SZ_OK)goto done;
 if(verify.NumFiles!=a.NumFiles){result=SZ_ERROR_ARCHIVE;goto done;}
 for(i=0;i<verify.NumFiles;i++){
  size_t bytes=2*(verify.FileNameOffsets[i+1]-verify.FileNameOffsets[i]);
  if(bytes!=offsets[i+1]-offsets[i]||memcmp(verify.FileNames+2*verify.FileNameOffsets[i],header.data+namesStart+offsets[i],bytes)){result=SZ_ERROR_ARCHIVE;goto done;}
 }
 SzArEx_Free(&verify,allocator);fclose(in.file);in.file=NULL;
 if(testProgress&&!testProgress(fileSize)){result=SZ_ERROR_PROGRESS;goto done;}
 /* Detect external changes before commit. DOS opens deny concurrent writers. */
#ifdef __WATCOMC__
 in.file=_fsopen(path,"rb",SH_DENYWR);
#else
 in.file=fopen(path,"rb");
#endif
 if(!in.file||stat(path,&current)||current.st_size!=original.st_size||current.st_mtime!=original.st_mtime){result=SZ_ERROR_READ;goto done;}
 processed=0;
 while(processed<fileSize){size_t n=fileSize-processed>4096?4096:(size_t)(fileSize-processed);
  if(testProgress&&!testProgress(processed)){result=SZ_ERROR_PROGRESS;goto done;}
  if(fread(copyBuf,1,n,in.file)!=n){result=SZ_ERROR_READ;goto done;}checkCRC=CrcUpdate(checkCRC,copyBuf,n);processed+=n;
 }
 if(sourceCRC!=checkCRC||fgetc(in.file)!=EOF||ferror(in.file)){result=SZ_ERROR_READ;goto done;}
 fclose(in.file);in.file=NULL;
 for(attempt=0;attempt<4096;attempt++){
  if(!renameSibling(path,backup,attempt,"BAK")){result=SZ_ERROR_UNSUPPORTED;goto done;}
  if(!renameBackup(path,backup)){backed=1;break;}
 }
 if(!backed){result=SZ_ERROR_WRITE;goto done;}
 if(rename(temporary,path)){
#ifdef __WATCOMC__
  if(rename(backup,path)){sprintf(renameMessage,"ERROR: Commit failed. The original archive is preserved in %s.",backup);goto commitFailed;}
#endif
  result=SZ_ERROR_WRITE;goto done;
 }
 committed=1;owned=0;
#ifdef __WATCOMC__
 if(_dos_setfileattr(path,attrs))strcpy(renameMessage,"WARNING: Names updated, but archive file attributes could not be restored.");
#else
 if(chmod(path,original.st_mode&0777))strcpy(renameMessage,"WARNING: Names updated, but archive file permissions could not be restored.");
#endif
 if(remove(backup))sprintf(renameMessage,"WARNING: Names updated. The previous archive remains in %s.",backup);
 backed=0;
 if(!renameMessage[0])sprintf(renameMessage,"Renamed %u archive entries. Packed data and other header properties were preserved.",changed);
 goto done;
#ifdef __WATCOMC__
 commitFailed:
 result=SZ_ERROR_WRITE;backed=0; /* Keep the recovery copy, never delete it. */
#endif
 done:
 if(in.file)fclose(in.file);if(out)fclose(out);
 if(owned&&remove(temporary)&&!renameMessage[0])strcpy(renameMessage,"ERROR: Update failed; the incomplete temporary file could not be removed.");
#ifndef __WATCOMC__
 if(backed&&!committed)remove(backup); /* Original path still exists on POSIX. */
#endif
 SzArEx_Free(&verify,allocator);SzArEx_Free(&a,allocator);
 ISzAlloc_Free(allocator,header.data);ISzAlloc_Free(allocator,offsets);ISzAlloc_Free(allocator,lookBuf);ISzAlloc_Free(allocator,copyBuf);
 return result;
}
#endif
