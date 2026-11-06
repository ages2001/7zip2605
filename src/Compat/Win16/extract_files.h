/* DOS/Win16 extraction policy shared by both frontends.
   Never overwrite an existing destination; reject names DOS cannot represent.
   The source archive is read-only and remains the metadata-preserving copy. */
#ifndef Z7_WIN16_EXTRACT_FILES_H
#define Z7_WIN16_EXTRACT_FILES_H
#include "dos_paths.h"
#include "extract_metadata.h"
static char extractMessage[512];
typedef struct {
 const CSzArEx *archive;
 char **paths;
 FILE *file;
 UInt32 current;
} ExtractOutput;
static SRes extractNameMode(const CSzArEx *archive,UInt32 index,char *name,int flat){
 size_t i=archive->FileNameOffsets[index],end=archive->FileNameOffsets[index+1];
 unsigned used=0,start=0;
 if(flat){size_t j;for(j=i;j+1<end;j++){unsigned ch=archive->FileNames[2*j]|((unsigned)archive->FileNames[2*j+1]<<8);if(ch=='/'||ch=='\\')i=j+1;}}
 for(;i+1<end;i++){
  unsigned ch=archive->FileNames[2*i]|((unsigned)archive->FileNames[2*i+1]<<8);
  if(used>=255||!ch||ch>=127)return SZ_ERROR_UNSUPPORTED;
  if(ch=='/'||ch=='\\'){
   if(!validDosComponent(name+start,used-start))return SZ_ERROR_UNSUPPORTED;
   name[used++]=Z7_PATH_SEPARATOR;start=used;
  }else name[used++]=(char)foldName(ch);
 }
 if(!validDosComponent(name+start,used-start))return SZ_ERROR_UNSUPPORTED;
 name[used]=0;return SZ_OK;
}
static SRes extractName(const CSzArEx *a,UInt32 i,char *name){return extractNameMode(a,i,name,0);}
static SRes extractWrite(void *context,UInt32 index,const void *data,size_t size){
 ExtractOutput *out=(ExtractOutput *)context;
 if(index>=out->archive->NumFiles)return SZ_ERROR_DATA;
 if(!out->paths[index])return SZ_OK;
 if(SzArEx_IsDir(out->archive,index))return SZ_ERROR_DATA;
 if(!out->file){out->file=fopen(out->paths[index],"wb");out->current=index;if(!out->file)return SZ_ERROR_WRITE;}
 if(out->current!=index)return SZ_ERROR_DATA;
 return fwrite(data,1,size,out->file)==size?SZ_OK:SZ_ERROR_WRITE;
}
static SRes extractEnd(void *context,UInt32 index){
 ExtractOutput *out=(ExtractOutput *)context;int failed;
 if(index>=out->archive->NumFiles)return SZ_ERROR_DATA;
 if(!out->paths[index])return SZ_OK;
 if(!out->file)return SzArEx_GetFileSize(out->archive,index)?SZ_ERROR_DATA:SZ_OK;
 if(out->current!=index)return SZ_ERROR_DATA;
 failed=fclose(out->file);out->file=NULL;return failed?SZ_ERROR_WRITE:SZ_OK;
}
static int makeExtractParents(char *path,size_t rootLength,int directory){
 size_t i;struct stat info;
 for(i=rootLength+1;path[i];i++)if(path[i]==Z7_PATH_SEPARATOR){
  path[i]=0;
  if(Z7_MKDIR(path)&& (stat(path,&info)||!(info.st_mode&S_IFDIR))){path[i]=Z7_PATH_SEPARATOR;return 0;}
  path[i]=Z7_PATH_SEPARATOR;
 }
 return !directory||!Z7_MKDIR(path)||(!stat(path,&info)&&(info.st_mode&S_IFDIR));
}
static int cleanExtract(const CSzArEx *archive,char **paths,const char *root,size_t rootLength){
 UInt32 i;unsigned depth;int failed=0;size_t length;char saved;
 for(i=0;i<archive->NumFiles;i++)if(paths[i]&&!SzArEx_IsDir(archive,i)){
  if(remove(paths[i])&&errno!=ENOENT)failed=1;
 }
 /* Remove implicit parent directories as well as explicit archive entries,
    deepest first, independently of archive member order. */
 for(depth=259;depth>rootLength;depth--)for(i=0;i<archive->NumFiles;i++){
  if(!paths[i])continue;length=strlen(paths[i]);if(length<depth)continue;
  if(paths[i][depth]==Z7_PATH_SEPARATOR||(length==depth&&SzArEx_IsDir(archive,i))){
   saved=paths[i][depth];paths[i][depth]=0;
   if(rmdir(paths[i])&&errno!=ENOENT)failed=1;
   paths[i][depth]=saved;
  }
 }
 if(rmdir(root))failed=1;return !failed;
}
/* A null mask means all entries. Otherwise one checked bit selects each
   archive entry. Unselected members still pass through CRC verification, but
   are never mapped to filesystem names or written. The caller owns the mask. */
static SRes extractArchiveSelected(CSzArEx *archive,Input *input,CLookToRead2 *look,ISzAllocPtr allocator,const char *destination,int flat,const Byte *selected,size_t selectedBytes){
 UInt32 i,j;size_t rootLength,bytes;SRes result=SZ_OK;int created=0;
 char root[260],name[260];ExtractOutput out;ArchiveSink sink;
 memset(&out,0,sizeof(out));extractMessage[0]=0;rootLength=strlen(destination);
 if(selected){
  UInt32 needed=archive->NumFiles/8+(archive->NumFiles%8!=0);int any=0;
  if((UInt64)selectedBytes<needed){strcpy(extractMessage,"ERROR: The archive selection is incomplete. No output was created.");return SZ_ERROR_PARAM;}
  for(i=0;i<archive->NumFiles;i++)if(selected[i/8]&(1u<<(i%8))){any=1;break;}
  if(!any){strcpy(extractMessage,"ERROR: Select at least one archive item. No output was created.");return SZ_ERROR_PARAM;}
 }
 if(!rootLength||rootLength>250){strcpy(extractMessage,"ERROR: Destination path is empty or too long.");return SZ_ERROR_UNSUPPORTED;}
 strcpy(root,destination);
 for(i=0;i<rootLength;i++)if(root[i]=='/'||root[i]=='\\')root[i]=Z7_PATH_SEPARATOR;
 while(rootLength&&root[rootLength-1]==Z7_PATH_SEPARATOR)root[--rootLength]=0;
 if(!rootLength){strcpy(extractMessage,"ERROR: Choose a new destination directory.");return SZ_ERROR_UNSUPPORTED;}
#ifdef __WATCOMC__
 if(!validDosDestination(root)){
  strcpy(extractMessage,"ERROR: Destination path must use valid DOS 8.3 directory names. No output was created.");
  return SZ_ERROR_UNSUPPORTED;
 }
#endif
 if(archive->NumFiles>(size_t)-1/sizeof(char *))return SZ_ERROR_MEM;
 bytes=(size_t)archive->NumFiles*sizeof(char *);
 out.paths=(char **)ISzAlloc_Alloc(allocator,bytes?bytes:1);if(!out.paths)return SZ_ERROR_MEM;
 out.archive=archive;memset(out.paths,0,bytes);
 for(i=0;i<archive->NumFiles;i++){
  if(selected&&!(selected[i/8]&(1u<<(i%8))))continue;
  if(flat&&SzArEx_IsDir(archive,i))continue;
  result=extractNameMode(archive,i,name,flat);
  if(result!=SZ_OK){sprintf(extractMessage,"ERROR: Archive item %lu has a path that DOS 8.3 cannot represent safely. The source archive is unchanged.",i+1);goto done;}
  bytes=rootLength+1+strlen(name)+1;
  if(bytes>260){result=SZ_ERROR_UNSUPPORTED;strcpy(extractMessage,"ERROR: Extracted path would exceed the supported path length.");goto done;}
  out.paths[i]=(char *)ISzAlloc_Alloc(allocator,bytes);if(!out.paths[i]){result=SZ_ERROR_MEM;goto done;}
  strcpy(out.paths[i],root);out.paths[i][rootLength]=Z7_PATH_SEPARATOR;strcpy(out.paths[i]+rootLength+1,name);
  for(j=0;j<i;j++)if(out.paths[j]&&(sameName(out.paths[j],out.paths[i])
      ||(!SzArEx_IsDir(archive,j)&&namePrefix(out.paths[j],out.paths[i]))
      ||(!SzArEx_IsDir(archive,i)&&namePrefix(out.paths[i],out.paths[j])))){
   result=SZ_ERROR_UNSUPPORTED;strcpy(extractMessage,"ERROR: Archive paths collide on a case-insensitive DOS filesystem.");goto done;
  }
 }
 /* Validate before creating output, then validate again while writing. */
 result=testArchive(archive,input,look,allocator);if(result!=SZ_OK)goto done;
 if(Z7_MKDIR(root)){result=SZ_ERROR_WRITE;strcpy(extractMessage,"ERROR: Destination must be a new directory with an existing parent. Existing files are never overwritten.");goto done;}
 created=1;
 for(i=0;i<archive->NumFiles;i++){
  if(!out.paths[i])continue;
  if(!makeExtractParents(out.paths[i],rootLength,SzArEx_IsDir(archive,i))){result=SZ_ERROR_WRITE;goto done;}
  if(!SzArEx_IsDir(archive,i)&&!SzArEx_GetFileSize(archive,i)){
   FILE *empty=fopen(out.paths[i],"wb");if(!empty){result=SZ_ERROR_WRITE;goto done;}
   if(fclose(empty)){result=SZ_ERROR_WRITE;goto done;}
  }
 }
 sink.context=&out;sink.Write=extractWrite;sink.End=extractEnd;
 result=processArchive(archive,input,look,allocator,&sink);
 if(result==SZ_OK){
  unsigned warning=input->extraMetadata?META_EXTRA:0,depth;
  /* Apply directories last, deepest first, so child creation cannot change
     their restored timestamps or encounter read-only parent directories. */
  for(i=0;i<archive->NumFiles;i++)if(out.paths[i]&&!SzArEx_IsDir(archive,i))warning|=extractMetadata(archive,i,out.paths[i]);
  for(depth=259;depth>rootLength;depth--)for(i=0;i<archive->NumFiles;i++)
   if(out.paths[i]&&SzArEx_IsDir(archive,i)&&strlen(out.paths[i])==depth)warning|=extractMetadata(archive,i,out.paths[i]);
  if(flat)for(i=0;i<archive->NumFiles;i++)if(SzArEx_IsDir(archive,i)&&(!selected||(selected[i/8]&(1u<<(i%8))))){warning|=16;break;}
  if(warning){
   strcpy(extractMessage,"WARNING: Some metadata could not be restored exactly:");
   if(warning&META_TIME)strcat(extractMessage," modification times (range, precision or system support);");
   if(warning&META_DIR_TIME)strcat(extractMessage," directory modification times (range, precision or system support);");
   if(warning&META_ATTR)strcat(extractMessage," file attributes;");
   if(warning&16)strcat(extractMessage," directory metadata (paths were omitted);");
   if(warning&META_EXTRA)strcat(extractMessage," creation times or archive-specific metadata;");
   strcat(extractMessage," original metadata remains in the unchanged source archive.");
  }
 }
 done:
 if(out.file){if(fclose(out.file)&&result==SZ_OK)result=SZ_ERROR_WRITE;out.file=NULL;}
 if(created&&result!=SZ_OK&&!cleanExtract(archive,out.paths,root,rootLength))strcpy(extractMessage,"ERROR: Extraction failed and some partial output could not be removed. Check the destination directory.");
 for(i=0;i<archive->NumFiles;i++)ISzAlloc_Free(allocator,out.paths[i]);
 ISzAlloc_Free(allocator,out.paths);return result;
}
static SRes extractArchiveMode(CSzArEx *archive,Input *input,CLookToRead2 *look,ISzAllocPtr allocator,const char *destination,int flat){
 return extractArchiveSelected(archive,input,look,allocator,destination,flat,NULL,0);
}
static SRes extractArchive(CSzArEx *archive,Input *input,CLookToRead2 *look,ISzAllocPtr allocator,const char *destination){
 return extractArchiveMode(archive,input,look,allocator,destination,0);
}
#endif
