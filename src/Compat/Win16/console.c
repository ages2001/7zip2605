/* Native DOS command front end for the shared Win16 archive backend.
   Scope: create/list/test/extract for 7z, ZIP, gzip and tar; xz read;
   7zAES-256 read; DOS 8.3 paths. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stream_folder.h"
#include "zip_archive.h"
#include "hash_file.h"
#include "../../C/7zCrc.h"
#ifndef Z7_GUI_FRONTEND
#include "command_control.h"
#endif
#ifdef __WATCOMC__
#include <dos.h>
#endif
/* Dynamic, wildcard-expanding source list for the DOS command line. Each entry
   is heap-allocated (freeSources releases them); there is no fixed count limit.
   '*' and '?' anywhere in a path are expanded via DOS findfirst against the
   file's own directory. Names without wildcards are taken literally. */
typedef struct {char **items;unsigned count,capacity;} SourceList;
static int sourcesPush(SourceList *l,const char *path,size_t len){
 char *copy;
 if(l->count==l->capacity){
  unsigned cap=l->capacity?l->capacity*2:16;char **grown;
  if(cap<=l->capacity)return 0;
  grown=(char **)realloc(l->items,(size_t)cap*sizeof(char *));
  if(!grown)return 0;l->items=grown;l->capacity=cap;
 }
 copy=(char *)malloc(len+1);if(!copy)return 0;
 memcpy(copy,path,len);copy[len]=0;l->items[l->count++]=copy;return 1;
}
static void freeSources(SourceList *l){unsigned i;for(i=0;i<l->count;i++)free(l->items[i]);free(l->items);l->items=NULL;l->count=l->capacity=0;}
static int sourceHasWild(const char *s){for(;*s;s++)if(*s=='*'||*s=='?')return 1;return 0;}
static int sourcesAdd(SourceList *l,const char *path){
#ifdef __WATCOMC__
 if(sourceHasWild(path)){
  struct find_t f;unsigned ok,dir=0;const char *p;char full[128];
  for(p=path;*p;p++)if(*p=='\\'||*p=='/'||*p==':')dir=(unsigned)(p-path)+1;
  if(dir>=sizeof(full))return 1;
  for(ok=_dos_findfirst(path,_A_RDONLY|_A_ARCH|_A_SUBDIR,&f);!ok;ok=_dos_findnext(&f)){
   if(f.name[0]=='.'&&(!f.name[1]||(f.name[1]=='.'&&!f.name[2])))continue; /* skip . and .. */
   {size_t nl=strlen(f.name);
    if(dir+nl<sizeof(full)){memcpy(full,path,dir);memcpy(full+dir,f.name,nl+1);
     if(!sourcesPush(l,full,dir+nl))return 0;}}
  }
  return 1;
 }
#endif
 return sourcesPush(l,path,strlen(path));
}
typedef struct {ISeekInStream iface;FILE *file;unsigned extraMetadata;} Input;
typedef struct ArchiveSink ArchiveSink;
struct ArchiveSink {
 void *context;
 SRes (*Write)(void *context,UInt32 file,const void *data,size_t size);
 SRes (*End)(void *context,UInt32 file);
};
typedef struct {
 ISeqOutStream iface;const CSzArEx *archive;UInt32 file,last,crc;
 UInt64 position;SRes error;ArchiveSink *sink;
} TestOutput;
static SRes readInput(ISeekInStreamPtr p,void *data,size_t *n){Input *s=(Input *)p;
#ifndef Z7_GUI_FRONTEND
 if(!commandProgress(0)){*n=0;return SZ_ERROR_PROGRESS;}
#endif
 *n=fread(data,1,*n,s->file);return ferror(s->file)?SZ_ERROR_READ:SZ_OK;}
static SRes seekInput(ISeekInStreamPtr p,Int64 *pos,ESzSeek origin){Input *s=(Input *)p;long n=(long)*pos;if((Int64)n!=*pos)return SZ_ERROR_UNSUPPORTED;if(fseek(s->file,n,(int)origin))return SZ_ERROR_READ;n=ftell(s->file);if(n<0)return SZ_ERROR_READ;*pos=n;return SZ_OK;}
static void *allocate(ISzAllocPtr p,size_t n){(void)p;return malloc(n);}
static void release(ISzAllocPtr p,void *v){(void)p;free(v);}
static int (*testProgress)(UInt64 position);
static SRes finishFile(TestOutput *out){
 if(SzBitWithVals_Check(&out->archive->CRCs,out->file)
     && CRC_GET_DIGEST(out->crc)!=out->archive->CRCs.Vals[out->file])return SZ_ERROR_CRC;
 if(out->sink){SRes result=out->sink->End(out->sink->context,out->file);if(result!=SZ_OK)return result;}
 out->file++;out->crc=CRC_INIT_VAL;return SZ_OK;
}
static size_t testOutput(ISeqOutStreamPtr p,const void *buffer,size_t n){
 TestOutput *out=(TestOutput *)p;const Byte *data=(const Byte *)buffer;size_t original=n;
 if(testProgress && !testProgress(out->position)){out->error=SZ_ERROR_PROGRESS;return 0;}
 while(n){
  UInt64 remaining;size_t take;
  while(out->file<out->last && out->position==out->archive->UnpackPositions[out->file+1]){
   out->error=finishFile(out);if(out->error!=SZ_OK)return 0;
  }
  if(out->file>=out->last){out->error=SZ_ERROR_DATA;return 0;}
  remaining=out->archive->UnpackPositions[out->file+1]-out->position;
  take=remaining<n?(size_t)remaining:n;
  if(out->sink){out->error=out->sink->Write(out->sink->context,out->file,data,take);if(out->error!=SZ_OK)return 0;}
  out->crc=CrcUpdate(out->crc,data,take);out->position+=take;data+=take;n-=take;
 }
 return original;
}
static SRes openArchive(CSzArEx *archive,Input *input,CLookToRead2 *look,ISzAllocPtr allocator){
 SRes result;input->extraMetadata=0;
 result=SzArEx_Open(archive,&look->vt,allocator,allocator);
 if(result==SZ_ERROR_NO_ARCHIVE){
  SzArEx_Free(archive,allocator);
  result=Z7Win16_OpenZip(archive,&input->iface,allocator,&input->extraMetadata);
 }
 return result;
}
static SRes processArchive(CSzArEx *archive,Input *input,CLookToRead2 *look,ISzAllocPtr allocator,ArchiveSink *sink){
 UInt32 folder,i;
 for(i=0;i<archive->NumFiles;i++)
  if(SzArEx_GetFileSize(archive,i)==0 && SzBitWithVals_Check(&archive->CRCs,i)
      && archive->CRCs.Vals[i]!=0)return SZ_ERROR_CRC;
 for(folder=0;folder<archive->db.NumFolders;folder++){
  TestOutput out;SRes result;
  out.iface.Write=testOutput;out.archive=archive;out.file=archive->FolderToFile[folder];
  out.last=archive->FolderToFile[folder+1];out.position=archive->UnpackPositions[out.file];
  out.crc=CRC_INIT_VAL;out.error=SZ_OK;out.sink=sink;
  result=Z7Win16_DecodeFolderProgress(archive,folder,&input->iface,&out.iface,allocator,testProgress);
  /* A fallback may restart only before the streaming decoder emitted data. */
  if(result==SZ_ERROR_UNSUPPORTED && out.position==archive->UnpackPositions[archive->FolderToFile[folder]]){
   UInt64 fullSize=SzAr_GetFolderUnpackSize(&archive->db,folder);size_t size=(size_t)fullSize;Byte *buffer;
   if((UInt64)size!=fullSize)return SZ_ERROR_MEM;
   buffer=(Byte *)ISzAlloc_Alloc(allocator,size?size:1);if(!buffer)return SZ_ERROR_MEM;
   LookToRead2_INIT(look);
   result=SzAr_DecodeFolder(&archive->db,folder,&look->vt,archive->dataPos,buffer,size,allocator);
   if(result==SZ_OK && testOutput(&out.iface,buffer,size)!=size)result=SZ_ERROR_WRITE;
   ISzAlloc_Free(allocator,buffer);
  }
  if(out.error!=SZ_OK)return out.error;
  if(result!=SZ_OK)return result;
  while(out.file<out.last && out.position==archive->UnpackPositions[out.file+1]){
   result=finishFile(&out);if(result!=SZ_OK)return result;
  }
  if(out.file!=out.last)return SZ_ERROR_DATA;
 }
 return SZ_OK;
}
static SRes testArchive(CSzArEx *archive,Input *input,CLookToRead2 *look,ISzAllocPtr allocator){
 return processArchive(archive,input,look,allocator,NULL);
}
#include "extract_files.h"
#include "archive_selection.h"
#include "create_archive.h"
#include "rename_archive.h"
#include "update_archive.h"
#include "create_zip.h"
#include "update_zip.h"
#include "gzip_format.h"
#include "tar_format.h"
#include "xz_format.h"
/* 7zAES password support lives in stream_folder.c (aes7z.h); the CLI only
   needs to set the password and recognise the "password required" code. */
#define SZ_ERROR_WIN16_PASSWORD 100
void aes7zSetPassword(const char *ascii);
#ifndef Z7_GUI_FRONTEND
static void printSize(UInt64 value){char text[24];unsigned n=0;do{text[n++]=(char)('0'+value%10);value/=10;}while(value);while(n)putchar(text[--n]);}
static void printName(const CSzArEx *archive,UInt32 index){
 size_t i,start=archive->FileNameOffsets[index],end=archive->FileNameOffsets[index+1];
 for(i=start;i+1<end;i++){
  unsigned ch=archive->FileNames[2*i]|((unsigned)archive->FileNames[2*i+1]<<8);
  if(ch>=32 && ch<127 && ch!='\\')putchar(ch);
  else if(ch=='\\')printf("\\\\");else printf("\\u%04X",ch);
 }
}
#endif
static const char *errorText(SRes result){switch(result){
 case SZ_ERROR_PROGRESS:return "Operation cancelled.";
 case SZ_ERROR_DATA:return "Invalid or incomplete compressed data.";
 case SZ_ERROR_MEM:return "Insufficient addressable memory for this archive's dictionary or metadata.";
 case SZ_ERROR_CRC:return "CRC verification failed.";
 case SZ_ERROR_UNSUPPORTED:return "This archive method or file offset is not supported by this build.";
 case SZ_ERROR_INPUT_EOF:return "Unexpected end of archive.";
 case SZ_ERROR_READ:return "Cannot read input or temporary dictionary data.";
 case SZ_ERROR_WRITE:return "Cannot write output or temporary data. Check free space and TEMP/TMP.";
 case SZ_ERROR_NO_ARCHIVE:return "Not a supported 7z or ZIP archive.";
 case SZ_ERROR_ARCHIVE:return "Invalid archive header.";
 default:return "Archive operation failed.";
}}
#ifndef Z7_GUI_FRONTEND
int main(int argc,char **argv){
 Input input;CLookToRead2 look;CSzArEx archive;ISzAlloc allocator={allocate,release};SRes result;Byte *buffer;UInt32 i;
 commandBegin();testProgress=commandProgress;
 puts("7-Zip 26.05 - native 16-bit edition (8086) - port v1.0");
 if(fflush(stdout)||ferror(stdout))return commandFinish(SZ_OK);
 /* Pull out any -pPASSWORD argument (7zAES) and compact argv so the rest of
    the positional command parsing is unaffected. */
 {int wi=1,ri;for(ri=1;ri<argc;ri++){if(argv[ri][0]=='-'&&argv[ri][1]=='p'){aes7zSetPassword(argv[ri]+2);
#ifdef Z7_WIN16_WRITE_XZ_AES
  aes7zEncSetPassword(argv[ri]+2);
#endif
  }else argv[wi++]=argv[ri];}argc=wi;}
 if(argc>=3&&!strcmp(argv[1],"h")){
  SourceList list={0,0,0};InputTree tree;unsigned mask=1,custom=0,m,j,k;int arg;Byte digest[64*Z7_WIN16_HASH_COUNT];UInt64 size;
  for(arg=2;arg<argc;arg++){
   if(!strncmp(argv[arg],"-scrc",5)){
    const char *name=argv[arg]+5;if(*name=='=')name++;if(!custom){custom=1;mask=0;}
    if(!strcmp(name,"*")){mask=(1u<<Z7_WIN16_HASH_COUNT)-1;continue;}
    for(m=0;m<Z7_WIN16_HASH_COUNT;m++)if(sameName(name,Z7Win16_HashMethods[m].name))break;
    if(m==Z7_WIN16_HASH_COUNT){puts("ERROR: Unknown checksum method.");freeSources(&list);return 7;}mask|=1u<<m;
   }else if(!sourcesAdd(&list,argv[arg])){puts("ERROR: Insufficient memory.");freeSources(&list);return 2;}
  }
  result=inputTreeBuild(&tree,list.count,(const char *const *)list.items,NULL,&allocator);
  if(result!=SZ_OK&&inputTreeMessage[0])puts(inputTreeMessage);
  for(i=0;result==SZ_OK&&i<tree.count;i++)if(!tree.items[i].directory){
   result=Z7Win16_HashFileMany(tree.items[i].source,mask,digest,&size,&allocator,commandProgress);k=0;
   if(result!=SZ_OK){printf("ERROR: Cannot calculate checksums for %s.\n",tree.items[i].source);break;}
   for(m=0;result==SZ_OK&&m<Z7_WIN16_HASH_COUNT;m++)if(mask&(1u<<m)){
    printf("%s ",Z7Win16_HashMethods[m].name);for(j=0;j<Z7Win16_HashMethods[m].digestSize;j++)printf("%02X",(unsigned)digest[64*k+j]);k++;
    putchar(' ');printSize(size);printf(" %s\n",tree.items[i].name);if(fflush(stdout))result=SZ_ERROR_WRITE;
   }
  }
  inputTreeFree(&tree,&allocator);freeSources(&list);if(result!=SZ_OK){printf("ERROR: %s (code %d)\n",errorText(result),result);return commandFinish(result);}
  puts("Everything is Ok");return commandFinish(SZ_OK);
 }
 if(argc==5&&!strcmp(argv[1],"rn")){
  CrcGenerateTable();result=renameAnyArchive(argv[2],argv[3],argv[4],&allocator);
  if(renameMessage[0])puts(renameMessage);
  if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);
  return commandFinish(result);
 }
 if(argc>=4&&!strcmp(argv[1],"d")){
  CrcGenerateTable();result=updateAnyArchive(argv[2],(unsigned)(argc-3),(const char * const *)(argv+3),&allocator,0,1);
  if(updateMessage[0])puts(updateMessage);
  if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);
  return commandFinish(result);
 }
 if(argc>=4&&!strcmp(argv[1],"a")){
  SourceList list={0,0,0};unsigned count;const char *const *sources;int arg,lzma=0,method=-1,zip=0;size_t length=strlen(argv[2]);struct stat existingArchive;int exists=!stat(argv[2],&existingArchive);
  if(gzipIsName(argv[2])){
   const char *source=NULL;int n=0;
   for(arg=3;arg<argc;arg++){if(!strncmp(argv[arg],"-m0=",4)||!strcmp(argv[arg],"-tzip")||!strcmp(argv[arg],"-t7z"))continue;source=argv[arg];n++;}
   if(n!=1){puts("ERROR: gzip stores exactly one file.");return 7;}
   CrcGenerateTable();result=gzipCreate(argv[2],source,&allocator);
   if(gzipMessage[0])puts(gzipMessage);
   if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);
   return commandFinish(result);
  }
  if(length>4&&sameName(argv[2]+length-4,".ZIP"))zip=1;
  if(exists)zip=archiveIsZip(argv[2]);
  for(arg=3;arg<argc;arg++){
   if(!strcmp(argv[arg],"-m0=lzma"))method=1;
   else if(!strcmp(argv[arg],"-m0=copy"))method=0;
   else if(!strcmp(argv[arg],"-m0=deflate"))method=2;
   else if(!strcmp(argv[arg],"-m0=deflate64"))method=3;
   else if(!strcmp(argv[arg],"-tzip"))zip=1;
   else if(!strcmp(argv[arg],"-t7z"))zip=0;
   else if(!sourcesAdd(&list,argv[arg])){puts("ERROR: Insufficient memory.");freeSources(&list);return 2;}
  }
  count=list.count;sources=(const char *const *)list.items;
  if(xzIsName(argv[2])){
#ifdef Z7_WIN16_WRITE_XZ_AES
   if(count!=1){puts("ERROR: xz stores exactly one file.");freeSources(&list);return 7;}
   CrcGenerateTable();result=xzCreate(argv[2],sources[0],&allocator);freeSources(&list);
   if(xzMessage[0])puts(xzMessage);
   if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);
   return commandFinish(result);
#else
   puts("ERROR: xz creation is not supported; xz archives can be listed, tested and extracted. Use .7z, .zip, .gz or .tar to create.");freeSources(&list);return 7;
#endif
  }
  if(tarIsName(argv[2])){CrcGenerateTable();result=tarCreate(argv[2],count,sources,&allocator);freeSources(&list);if(tarMessage[0])puts(tarMessage);if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);return commandFinish(result);}
  if(exists&&zip!=archiveIsZip(argv[2])){puts("ERROR: Requested format does not match the existing archive.");freeSources(&list);return 7;}
  if((zip&&method==1)||(!zip&&method>=2)){puts("ERROR: Compression method does not match the archive format.");freeSources(&list);return 7;}
  lzma=method==1;
#ifdef Z7_WIN16_WRITE_XZ_AES
  if(aes7zEncHasPassword()&&!zip)lzma=1;
#endif
  CrcGenerateTable();
  if(zip){struct stat existing;if(!stat(argv[2],&existing)){result=updateZip(argv[2],count,sources,&allocator,method==3?2:method!=0,0);if(updateMessage[0])puts(updateMessage);}else{result=createZip(argv[2],count,sources,&allocator,method==3?2:method!=0);if(zipCreateMessage[0])puts(zipCreateMessage);}}
  else {struct stat existing;if(!stat(argv[2],&existing)){
   result=updateAnyArchive(argv[2],count,sources,&allocator,lzma,0);if(updateMessage[0])puts(updateMessage);
  }else {result=createArchiveMethod(argv[2],count,sources,&allocator,lzma);if(createMessage[0])puts(createMessage);}}
  freeSources(&list);
  if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);
  return commandFinish(result);
 }
 if(!((argc==3&&(!strcmp(argv[1],"l")||!strcmp(argv[1],"t")))||(argc>=4&&(!strcmp(argv[1],"x")||!strcmp(argv[1],"e"))))){
  puts("Usage: 7Z16C h [-scrcMETHOD|-scrc*] file-or-folder...\n       7Z16C l|t archive.7z\n       7Z16C x|e archive.7z|zip new-directory [entry-name...]\n       7Z16C a archive.7z [-m0=copy|-m0=lzma] file-or-folder...\n       7Z16C a archive.zip [-m0=copy|-m0=deflate|-m0=deflate64] file-or-folder...\n       7Z16C a archive.gz file   (gzip: one file)\n       7Z16C a archive.tar file-or-folder...   (tar: store)\n       7Z16C rn archive.7z old-name new-name\n       7Z16C d archive.7z|zip name...\nCommands: rn = rename, a = add/update, d = delete, l = list, t = test, x = extract with paths, e = extract without paths");return 7;
 }
 input.iface.Read=readInput;input.iface.Seek=seekInput;input.file=fopen(argv[2],"rb");if(!input.file){puts("ERROR: Cannot open the archive.");return 2;}
 if(gzipIsName(argv[2])||gzipMagicFile(input.file)){
  CrcGenerateTable();
  if(argv[1][0]=='l')result=gzipList(input.file,argv[2]);
  else if(argv[1][0]=='t')result=gzipTest(input.file,&allocator);
  else result=gzipExtract(input.file,argv[2],argv[3],&allocator);
  if(gzipMessage[0])puts(gzipMessage);
  if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);else puts("Everything is Ok");
  fclose(input.file);return commandFinish(result);
 }
 if(tarIsName(argv[2])||tarMagicFile(input.file)){
  CrcGenerateTable();
  if(argv[1][0]=='l'){puts("Type Size Name (non-ASCII characters use Unicode escapes)");result=tarList(input.file);}
  else if(argv[1][0]=='t')result=tarTest(input.file,&allocator);
  else result=tarExtract(input.file,argv[3],argv[1][0]=='e',&allocator);
  if(tarMessage[0])puts(tarMessage);
  if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);else puts("Everything is Ok");
  fclose(input.file);return commandFinish(result);
 }
 if(xzIsName(argv[2])||xzMagicFile(input.file)){
  CrcGenerateTable();
  if(argv[1][0]=='l'){puts("Type Size Name (non-ASCII characters use Unicode escapes)");result=xzList(input.file,argv[2]);}
  else if(argv[1][0]=='t')result=xzTest(input.file,&allocator);
  else result=xzExtract(input.file,argv[2],argv[3],&allocator);
  if(xzMessage[0])puts(xzMessage);
  if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);else puts("Everything is Ok");
  fclose(input.file);return commandFinish(result);
 }
 buffer=(Byte *)malloc(4096);if(!buffer){fclose(input.file);puts("ERROR: Insufficient memory.");return 2;}
 LookToRead2_CreateVTable(&look,False);look.realStream=&input.iface;look.buf=buffer;look.bufSize=4096;LookToRead2_INIT(&look);SzArEx_Init(&archive);CrcGenerateTable();
 result=openArchive(&archive,&input,&look,&allocator);
 if(result==SZ_OK){
  if(argv[1][0]=='l'){
   puts("Type Size Name (non-ASCII characters use Unicode escapes)");
   for(i=0;i<archive.NumFiles;i++){
    if(!commandProgress(i)){result=SZ_ERROR_PROGRESS;break;}
    printf("%c ",SzArEx_IsDir(&archive,i)?'D':'F');printSize(SzArEx_GetFileSize(&archive,i));putchar(' ');printName(&archive,i);putchar('\n');
   }
  }else if(argv[1][0]=='x'||argv[1][0]=='e'){
   Byte *selection=NULL;size_t selectionBytes=0;
   if(argc>4){
    result=archiveSelectionNames(&archive,(unsigned)(argc-4),(const char *const *)(argv+4),&allocator,&selection,&selectionBytes);
    if(result==SZ_ERROR_MEM)puts("ERROR: Insufficient memory for entry selection. No output was created.");
    else if(result!=SZ_OK)puts("ERROR: Entry selectors must name existing ASCII archive paths, folders, or * / ? wildcard patterns that match at least one entry. No output was created.");
   }
   if(result==SZ_OK)result=extractArchiveSelected(&archive,&input,&look,&allocator,argv[3],argv[1][0]=='e',selection,selectionBytes);
   ISzAlloc_Free(&allocator,selection);
   if(extractMessage[0])puts(extractMessage);
  }else result=testArchive(&archive,&input,&look,&allocator);
 }
 if(result==SZ_ERROR_WIN16_PASSWORD)puts("ERROR: This archive is encrypted. Supply the password with -pPASSWORD.");
 else if(result!=SZ_OK)printf("ERROR: %s (code %d)\n",errorText(result),result);else puts("Everything is Ok");
 SzArEx_Free(&archive,&allocator);free(buffer);fclose(input.file);return commandFinish(result);
}

#endif
