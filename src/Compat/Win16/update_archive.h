/* Native content update transaction. Never drop unrecognized file metadata.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_UPDATE_ARCHIVE_H
#define Z7_WIN16_UPDATE_ARCHIVE_H
#define UPDATE_MAX 4096
static char updateMessage[320];
typedef struct {
 int oldIndex;const char *source,*addedName;
 Byte directory,method,props[5];long scratchOffset;UInt64 size,packed,mtime,stagedSize;UInt32 crc,decodedCRC,attributes;
} UpdateEntry;
typedef struct {
 CSzArEx *archive;UpdateEntry *entries;unsigned count;
 FILE *output,*scratch;char scratchPath[260];int scratchOwned;
 ISzAllocPtr allocator;int lzma;UInt64 progress;
} UpdateSink;
static SRes updateNumber(const Byte **data,const Byte *end,UInt64 *value){
 unsigned mask=0x80,i;Byte first;UInt64 n=0;
 if(*data==end)return SZ_ERROR_ARCHIVE;first=*(*data)++;
 for(i=0;i<8;i++){
  if(!(first&mask)){n|=(UInt64)(first&(mask-1))<<(8*i);*value=n;return SZ_OK;}
  if(*data==end)return SZ_ERROR_ARCHIVE;n|=(UInt64)*(*data)++<<(8*i);mask>>=1;
 }
 *value=n;return SZ_OK;
}
/* The parser exposes decoded known properties. Unknown indexed properties
   cannot be safely remapped when entries are added/deleted, so fail before
   creating output rather than silently discarding them. */
static SRes updateProperties(const CSzArEx *a){
 const Byte *p,*end;
 if(!a->SavedHeader||!a->FilePropertiesOffset||a->FilePropertiesOffset>=a->FilePropertiesEnd||a->FilePropertiesEnd>a->SavedHeaderSize)return SZ_ERROR_ARCHIVE;
 p=a->SavedHeader+a->FilePropertiesOffset;end=a->SavedHeader+a->FilePropertiesEnd;
 if(a->HasAdditionalStreams||a->FilePropertiesEnd+1!=a->SavedHeaderSize||a->SavedHeader[a->FilePropertiesEnd]!=0)return SZ_ERROR_UNSUPPORTED;
 while(p<end){UInt64 id,size;RINOK(updateNumber(&p,end,&id))if(!id)return p==end?SZ_OK:SZ_ERROR_ARCHIVE;
  RINOK(updateNumber(&p,end,&size))if(size>(UInt64)(end-p))return SZ_ERROR_ARCHIVE;
  if(id==25){size_t k;for(k=0;k<(size_t)size;k++)if(p[k])return SZ_ERROR_UNSUPPORTED;}
  else if(id!=14&&id!=15&&id!=17&&id!=18&&id!=20&&id!=21)return SZ_ERROR_UNSUPPORTED;
  p+=(size_t)size;
 }
 return SZ_ERROR_ARCHIVE;
}
static int updateFind(const UpdateSink *s,unsigned old){unsigned i;for(i=0;i<s->count;i++)if(s->entries[i].oldIndex==(int)old)return (int)i;return -1;}
static SRes updateEncode(UpdateSink *s,UpdateEntry *e,FILE *input,int requireEOF){
 SRes result=SZ_OK;Byte *buffer=NULL;UInt64 left=e->size;UInt32 crc=CRC_INIT_VAL;
 if(e->directory)return SZ_OK;
 if(s->lzma&&left){e->method=1;result=createLzmaRange(input,s->output,left,s->progress,&e->packed,&e->crc,e->props,s->allocator,requireEOF);s->progress+=left;return result;}
 e->method=0;e->packed=left;
 buffer=(Byte *)ISzAlloc_Alloc(s->allocator,4096);if(!buffer)return SZ_ERROR_MEM;
 while(left){size_t n=left>4096?4096:(size_t)left;
  if(testProgress&&!testProgress(s->progress)){result=SZ_ERROR_PROGRESS;break;}
  if(fread(buffer,1,n,input)!=n){result=SZ_ERROR_READ;break;}
  if(!createWriteBytes(s->output,buffer,n)){result=SZ_ERROR_WRITE;break;}
  crc=CrcUpdate(crc,buffer,n);left-=n;s->progress+=n;
 }
 if(result==SZ_OK&&requireEOF&&(fgetc(input)!=EOF||ferror(input)))result=SZ_ERROR_READ;
 e->crc=CRC_GET_DIGEST(crc);ISzAlloc_Free(s->allocator,buffer);return result;
}
/* Stage retained data before starting the encoder. Decoder and encoder
   working sets must not coexist in Windows 3.0 conventional memory. */
static SRes updateWrite(void *context,UInt32 index,const void *data,size_t size){
 UpdateSink *s=(UpdateSink *)context;int entry=updateFind(s,(unsigned)index);UpdateEntry *e;
 if(entry<0||s->entries[entry].source)return SZ_OK;e=s->entries+entry;
 if(!s->scratch){int fd=open(s->scratchPath,O_RDWR|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);if(fd<0)return SZ_ERROR_WRITE;s->scratchOwned=1;s->scratch=fdopen(fd,"w+b");if(!s->scratch){close(fd);return SZ_ERROR_WRITE;}}
 if(!e->stagedSize&&(e->scratchOffset=ftell(s->scratch))<0)return SZ_ERROR_WRITE;
 if(!createWriteBytes(s->scratch,data,size))return SZ_ERROR_WRITE;
 e->decodedCRC=CrcUpdate(e->decodedCRC,data,size);e->stagedSize+=size;return SZ_OK;
}
static SRes updateEnd(void *context,UInt32 index){
 UpdateSink *s=(UpdateSink *)context;int entry=updateFind(s,(unsigned)index);
 if(entry>=0&&!s->entries[entry].source&&s->entries[entry].stagedSize!=s->entries[entry].size)return SZ_ERROR_DATA;
 return SZ_OK;
}
static void updateNames(CreateHeader *h,const UpdateSink *s){
 unsigned i,j;UInt64 bytes=1;
 for(i=0;i<s->count;i++){const UpdateEntry *e=s->entries+i;bytes+=e->oldIndex>=0?2*(s->archive->FileNameOffsets[e->oldIndex+1]-s->archive->FileNameOffsets[e->oldIndex]):2*(strlen(e->addedName)+1);}
 createByte(h,17);createUInt(h,bytes);createByte(h,0);
 for(i=0;i<s->count;i++){const UpdateEntry *e=s->entries+i;
  if(e->oldIndex>=0){size_t begin=2*s->archive->FileNameOffsets[e->oldIndex],end=2*s->archive->FileNameOffsets[e->oldIndex+1];while(begin<end)createByte(h,s->archive->FileNames[begin++]);}
  else for(j=0;;j++){createLE(h,(unsigned char)e->addedName[j],2);if(!e->addedName[j])break;}
 }
}
static int updateDefined(const UpdateSink *s,unsigned i,unsigned id){const UpdateEntry *e=s->entries+i;if(e->source&&id!=18)return 1;return e->oldIndex>=0&&(id==18?SzBitWithVals_Check(&s->archive->CTime,e->oldIndex):id==20?SzBitWithVals_Check(&s->archive->MTime,e->oldIndex):SzBitWithVals_Check(&s->archive->Attribs,e->oldIndex));}
static void updateMetadata(CreateHeader *h,const UpdateSink *s,unsigned id){
 unsigned i,j,count=0,unit=id==21?4:8;for(i=0;i<s->count;i++)if(updateDefined(s,i,id))count++;if(!count)return;
 createByte(h,id);createUInt(h,2+(s->count+7)/8+(UInt64)unit*count);createByte(h,0);
 for(i=0;i<s->count;i+=8){unsigned bits=0;for(j=0;j<8&&i+j<s->count;j++)if(updateDefined(s,i+j,id))bits|=0x80>>j;createByte(h,bits);}
 createByte(h,0);
 for(i=0;i<s->count;i++)if(updateDefined(s,i,id)){const UpdateEntry *e=s->entries+i;UInt64 value;
  if(e->source&&id!=18)value=id==21?e->attributes:e->mtime;
  else if(id==21)value=s->archive->Attribs.Vals[e->oldIndex];
  else {const CNtfsFileTime *t=id==18?s->archive->CTime.Vals+e->oldIndex:s->archive->MTime.Vals+e->oldIndex;value=((UInt64)t->High<<32)|t->Low;}
  createLE(h,value,unit);
 }
}
static SRes updateHeader(CreateHeader *h,const UpdateSink *s){
 unsigned i,j,streams=0,empty=0,bit=0,value=0;
 for(i=0;i<s->count;i++)if(s->entries[i].size)streams++;else empty++;
 createByte(h,1);
 if(streams){
  createByte(h,4);createByte(h,6);createUInt(h,0);createUInt(h,streams);createByte(h,9);
  for(i=0;i<s->count;i++)if(s->entries[i].size)createUInt(h,s->entries[i].packed);
  createByte(h,0);createByte(h,7);createByte(h,11);createUInt(h,streams);createByte(h,0);
  for(i=0;i<s->count;i++)if(s->entries[i].size){const UpdateEntry *e=s->entries+i;createUInt(h,1);
   if(e->method){createByte(h,0x23);createByte(h,3);createByte(h,1);createByte(h,1);createUInt(h,5);for(j=0;j<5;j++)createByte(h,e->props[j]);}
   else {createByte(h,1);createByte(h,0);}
  }
  createByte(h,12);for(i=0;i<s->count;i++)if(s->entries[i].size)createUInt(h,s->entries[i].size);
  createByte(h,10);createByte(h,1);for(i=0;i<s->count;i++)if(s->entries[i].size)createLE(h,s->entries[i].crc,4);
  createByte(h,0);createByte(h,0);
 }
 createByte(h,5);createUInt(h,s->count);
 if(empty){createByte(h,14);createUInt(h,(s->count+7)/8);
  for(i=0;i<s->count;i+=8){unsigned bits=0;for(j=0;j<8&&i+j<s->count;j++)if(!s->entries[i+j].size)bits|=0x80>>j;createByte(h,bits);}
  createByte(h,15);createUInt(h,(empty+7)/8);
  for(i=0;i<s->count;i++)if(!s->entries[i].size){if(!s->entries[i].directory)value|=0x80>>bit;if(++bit==8){createByte(h,value);bit=value=0;}}
  if(bit)createByte(h,value);
 }
 updateNames(h,s);updateMetadata(h,s,18);updateMetadata(h,s,20);updateMetadata(h,s,21);createByte(h,0);createByte(h,0);
 return h->failed?SZ_ERROR_MEM:SZ_OK;
}
static SRes updateVerify(const CSzArEx *a,const UpdateSink *s){
 unsigned i,j;
 if(a->NumFiles!=s->count)return SZ_ERROR_ARCHIVE;
 for(i=0;i<s->count;i++){
  const UpdateEntry *e=s->entries+i;const Byte *name=a->FileNames+2*a->FileNameOffsets[i];
  size_t size=2*(a->FileNameOffsets[i+1]-a->FileNameOffsets[i]);
  if(SzArEx_GetFileSize(a,i)!=e->size||!!SzArEx_IsDir(a,i)!=!!e->directory)return SZ_ERROR_ARCHIVE;
  if(e->oldIndex>=0){size_t oldSize=2*(s->archive->FileNameOffsets[e->oldIndex+1]-s->archive->FileNameOffsets[e->oldIndex]);
   if(size!=oldSize||memcmp(name,s->archive->FileNames+2*s->archive->FileNameOffsets[e->oldIndex],size))return SZ_ERROR_ARCHIVE;
  }else{
   if(size!=2*(strlen(e->addedName)+1))return SZ_ERROR_ARCHIVE;
   for(j=0;j<size/2;j++)if(renameChar(name+2*j)!=(unsigned char)e->addedName[j])return SZ_ERROR_ARCHIVE;
  }
 }
 return SZ_OK;
}
static SRes updateFileCRC(FILE *file,Byte *buffer,UInt32 *crc){
 size_t n;UInt64 processed=0;*crc=CRC_INIT_VAL;rewind(file);
 while((n=fread(buffer,1,4096,file))!=0){if(testProgress&&!testProgress(processed))return SZ_ERROR_PROGRESS;*crc=CrcUpdate(*crc,buffer,n);processed+=n;}
 return ferror(file)?SZ_ERROR_READ:SZ_OK;
}
static SRes updateCommit(const char *path,const char *candidate,const struct stat *before,UInt32 expected,Byte *buffer,int *owned){
 FILE *file;UInt32 crc;struct stat now;unsigned attempt,attrs=0;char backup[260];int backed=0;SRes result;
#ifdef __WATCOMC__
 file=_fsopen(path,"rb",SH_DENYWR);if(_dos_getfileattr(path,&attrs)||(attrs&1)){if(file)fclose(file);return SZ_ERROR_WRITE;}
#else
 file=fopen(path,"rb");
#endif
 if(!file)return SZ_ERROR_READ;result=updateFileCRC(file,buffer,&crc);
 if(result==SZ_OK&&(stat(path,&now)||now.st_size!=before->st_size||now.st_mtime!=before->st_mtime||crc!=expected))result=SZ_ERROR_READ;
 fclose(file);if(result!=SZ_OK)return result;
 for(attempt=0;attempt<4096;attempt++){if(!renameSibling(path,backup,attempt,"BAK"))return SZ_ERROR_UNSUPPORTED;if(!renameBackup(path,backup)){backed=1;break;}}
 if(!backed)return SZ_ERROR_WRITE;
 if(rename(candidate,path)){
#ifdef __WATCOMC__
  if(rename(backup,path))sprintf(updateMessage,"ERROR: Commit and restore failed. The original archive is preserved in %s.",backup);
#else
  remove(backup);
#endif
  return SZ_ERROR_WRITE;
 }
 *owned=0;
#ifdef __WATCOMC__
 if(_dos_setfileattr(path,attrs))strcpy(updateMessage,"WARNING: Update completed, but archive file attributes could not be restored.");
#else
 if(chmod(path,before->st_mode&0777))strcpy(updateMessage,"WARNING: Update completed, but archive permissions could not be restored.");
#endif
 if(remove(backup))sprintf(updateMessage,"WARNING: Update completed. The previous archive remains in %s.",backup);
 return SZ_OK;
}
static SRes updateArchive(const char *path,unsigned count,const char * const *sources,ISzAllocPtr allocator,int lzma,int deleting){
 Input in;CLookToRead2 look;CSzArEx a,verify;UpdateSink sink;ArchiveSink callbacks;CreateHeader header,start;
 Byte *lookBuf=NULL,signature[32];unsigned i,j,attempt,capacity,changed=0;int owned=0,fd;
 InputTree tree;SRes result=SZ_OK;UInt32 originalCRC=0;struct stat original,st;char candidate[260];long packed;
 memset(&tree,0,sizeof(tree));memset(&sink,0,sizeof(sink));memset(&header,0,sizeof(header));in.file=NULL;candidate[0]=0;updateMessage[0]=0;SzArEx_Init(&a);SzArEx_Init(&verify);
 if(!count||count>UPDATE_MAX)return SZ_ERROR_UNSUPPORTED;
#ifdef __WATCOMC__
 if(!validDosDestination(path))return SZ_ERROR_UNSUPPORTED;in.file=_fsopen(path,"rb",SH_DENYWR);
#else
 in.file=fopen(path,"rb");
#endif
 if(!in.file||stat(path,&original)){result=SZ_ERROR_READ;goto done;}
 if(!(original.st_mode&S_IWUSR)){result=SZ_ERROR_WRITE;goto done;}
 /* Identity checks and archive reads never overlap. Reuse their buffer so
    the encoder does not retain a second, idle 4 KiB allocation in real mode.
    Read sizes, compression history and all verification remain unchanged. */
 lookBuf=(Byte *)ISzAlloc_Alloc(allocator,4096);
 if(!lookBuf){result=SZ_ERROR_MEM;goto done;}
 result=updateFileCRC(in.file,lookBuf,&originalCRC);if(result!=SZ_OK)goto done;rewind(in.file);
 in.iface.Read=readInput;in.iface.Seek=seekInput;in.extraMetadata=0;LookToRead2_CreateVTable(&look,False);look.realStream=&in.iface;look.buf=lookBuf;look.bufSize=4096;LookToRead2_INIT(&look);
 a.SaveHeader=True;result=SzArEx_Open(&a,&look.vt,allocator,allocator);if(result!=SZ_OK)goto done;
 if(!a.SavedHeader||a.NumFiles>UPDATE_MAX){result=SZ_ERROR_UNSUPPORTED;goto done;}
 result=updateProperties(&a);if(result!=SZ_OK){strcpy(updateMessage,"ERROR: This archive has metadata that cannot yet be remapped safely. The original archive was not changed.");goto done;}
 /* Archive-level properties are not discarded implicitly. */
 if(a.SavedHeaderSize<2|| (a.SavedHeader[1]!=4&&a.SavedHeader[1]!=5)){result=SZ_ERROR_UNSUPPORTED;goto done;}
 if(!deleting){result=inputTreeBuild(&tree,count,sources,path,allocator);if(result!=SZ_OK){strcpy(updateMessage,inputTreeMessage);goto done;}count=tree.count;}
 for(i=0;i<a.NumFiles;i++)for(j=0;j<i;j++)if(renameCollision(a.FileNames+2*a.FileNameOffsets[i],a.FileNames+2*a.FileNameOffsets[j],SzArEx_IsDir(&a,i),SzArEx_IsDir(&a,j))){result=SZ_ERROR_UNSUPPORTED;goto done;}
 capacity=(unsigned)a.NumFiles+(deleting?0:count);if(capacity>UPDATE_MAX)capacity=UPDATE_MAX;if(!capacity)capacity=1;
 sink.entries=(UpdateEntry *)ISzAlloc_Alloc(allocator,capacity*sizeof(UpdateEntry));if(!sink.entries){result=SZ_ERROR_MEM;goto done;}memset(sink.entries,0,capacity*sizeof(UpdateEntry));
 sink.archive=&a;sink.allocator=allocator;sink.lzma=lzma;
 for(i=0;i<a.NumFiles;i++){UpdateEntry *e=sink.entries+sink.count;int removeEntry=0;
  if(deleting)for(j=0;j<count;j++){if(!renamePath(sources[j])){result=SZ_ERROR_UNSUPPORTED;goto done;}if(renameMatch(a.FileNames+2*a.FileNameOffsets[i],sources[j],1)){removeEntry=1;break;}}
  if(removeEntry){changed++;continue;}
  e->oldIndex=(int)i;e->decodedCRC=CRC_INIT_VAL;e->directory=(Byte)SzArEx_IsDir(&a,i);e->size=SzArEx_GetFileSize(&a,i);sink.count++;
 }
 if(!deleting)for(j=0;j<count;j++){
  const InputNode *node=tree.items+j;const char *name=node->name;UpdateEntry *e=NULL;Byte wide[520];unsigned k;
  for(k=0;k<=strlen(name);k++){wide[2*k]=(Byte)name[k];wide[2*k+1]=0;}
  for(i=0;i<sink.count;i++){
   UpdateEntry *item=sink.entries+i;
   if(item->oldIndex>=0){const Byte *old=a.FileNames+2*a.FileNameOffsets[item->oldIndex];
    if(renameCollision(old,wide,item->directory,node->directory)){if(!inputTreeWideMatch(old,name)||!!item->directory!=!!node->directory){result=SZ_ERROR_UNSUPPORTED;goto done;}e=item;break;}
   }else if(sameName(item->addedName,name)){e=item;break;}
  }
  if(e&&e->source){strcpy(updateMessage,"ERROR: Input names collide. The archive was not changed.");result=SZ_ERROR_UNSUPPORTED;goto done;}
  if(!e){if(sink.count==UPDATE_MAX){result=SZ_ERROR_UNSUPPORTED;goto done;}e=sink.entries+sink.count++;e->oldIndex=-1;e->addedName=name;}
  if(stat(node->source,&st)){result=SZ_ERROR_READ;goto done;}if((st.st_mode&S_IFMT)!=(node->directory?S_IFDIR:S_IFREG)||st.st_size<0){result=SZ_ERROR_UNSUPPORTED;goto done;}
  e->source=node->source;e->directory=node->directory;e->size=node->directory?0:(UInt64)st.st_size;e->mtime=((UInt64)((Int64)st.st_mtime+11644473600LL))*10000000UL;e->attributes=(node->directory?0x10:0x20)|((st.st_mode&S_IWUSR)?0:1);
#ifdef __WATCOMC__
  {unsigned attr;if(_dos_getfileattr(node->source,&attr)){result=SZ_ERROR_READ;goto done;}e->attributes=attr;}
#endif
  changed++;
 }
 if(!changed){strcpy(updateMessage,"No matching archive entries. The archive is unchanged.");goto done;}
 for(attempt=0;attempt<4096;attempt++){if(!renameSibling(path,candidate,attempt,"TMP")){result=SZ_ERROR_UNSUPPORTED;goto done;}fd=open(candidate,O_RDWR|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);if(fd>=0)break;}
 if(attempt==4096){result=SZ_ERROR_WRITE;goto done;}owned=1;sink.output=fdopen(fd,"w+b");if(!sink.output){close(fd);result=SZ_ERROR_WRITE;goto done;}
 for(attempt=0;attempt<4096;attempt++){if(!renameSibling(path,sink.scratchPath,attempt,"DAT")){result=SZ_ERROR_UNSUPPORTED;goto done;}if(stat(sink.scratchPath,&st))break;}
 if(attempt==4096){result=SZ_ERROR_WRITE;goto done;}
 memset(signature,0,32);if(!createWriteBytes(sink.output,signature,32)){result=SZ_ERROR_WRITE;goto done;}
 callbacks.context=&sink;callbacks.Write=updateWrite;callbacks.End=updateEnd;
 result=processArchive(&a,&in,&look,allocator,&callbacks);if(result!=SZ_OK)goto done;
 if(sink.scratch&&fflush(sink.scratch)){result=SZ_ERROR_WRITE;goto done;}
 /* Retained input is now in the private staging file. No archive reads occur
    while the encoder runs, so its working set must not keep this buffer. */
 ISzAlloc_Free(allocator,lookBuf);lookBuf=NULL;look.buf=NULL;
 for(i=0;i<sink.count;i++){
  UpdateEntry *e=sink.entries+i;FILE *source;
  if(e->directory)continue;
  if(e->source){source=fopen(e->source,"rb");if(!source){result=SZ_ERROR_READ;goto done;}}
  else {
   if(e->stagedSize!=e->size){result=SZ_ERROR_DATA;goto done;}
   if(!e->size)continue;source=sink.scratch;
   if(!source||fseek(source,e->scratchOffset,SEEK_SET)){result=SZ_ERROR_READ;goto done;}
  }
  result=updateEncode(&sink,e,source,e->source!=NULL);
  if(e->source){if(fclose(source)&&result==SZ_OK)result=SZ_ERROR_READ;}
  else if(result==SZ_OK&&e->crc!=CRC_GET_DIGEST(e->decodedCRC))result=SZ_ERROR_CRC;
  if(result!=SZ_OK)goto done;
 }
 if(sink.scratch){if(fclose(sink.scratch)){sink.scratch=NULL;result=SZ_ERROR_WRITE;goto done;}sink.scratch=NULL;}
 if(sink.scratchOwned){if(remove(sink.scratchPath)){result=SZ_ERROR_WRITE;goto done;}sink.scratchOwned=0;}

 packed=ftell(sink.output);if(packed<32){result=SZ_ERROR_WRITE;goto done;}packed-=32;
 header.data=(Byte *)ISzAlloc_Alloc(allocator,CREATE_HEADER_CAPACITY);if(!header.data){result=SZ_ERROR_MEM;goto done;}
 result=updateHeader(&header,&sink);if(result!=SZ_OK)goto done;
 if(!createWriteBytes(sink.output,header.data,header.used)){result=SZ_ERROR_WRITE;goto done;}
 memcpy(signature,"7z\xBC\xAF\x27\x1C",6);signature[6]=0;signature[7]=4;start.data=signature;start.used=12;start.failed=0;
 createLE(&start,(UInt64)packed,8);createLE(&start,header.used,8);createLE(&start,CrcCalc(header.data,header.used),4);start.used=8;createLE(&start,CrcCalc(signature+12,20),4);
 if(fseek(sink.output,0,SEEK_SET)||!createWriteBytes(sink.output,signature,32)||fflush(sink.output)){result=SZ_ERROR_WRITE;goto done;}
 if(fclose(sink.output)){sink.output=NULL;result=SZ_ERROR_WRITE;goto done;}sink.output=NULL;fclose(in.file);in.file=NULL;
 ISzAlloc_Free(allocator,header.data);header.data=NULL;
 lookBuf=(Byte *)ISzAlloc_Alloc(allocator,4096);if(!lookBuf){result=SZ_ERROR_MEM;goto done;}look.buf=lookBuf;
 in.file=fopen(candidate,"rb");if(!in.file){result=SZ_ERROR_READ;goto done;}LookToRead2_INIT(&look);result=SzArEx_Open(&verify,&look.vt,allocator,allocator);if(result!=SZ_OK)goto done;
 result=updateVerify(&verify,&sink);if(result!=SZ_OK)goto done;result=testArchive(&verify,&in,&look,allocator);if(result!=SZ_OK)goto done;
 fclose(in.file);in.file=NULL;SzArEx_Free(&verify,allocator);
 result=updateCommit(path,candidate,&original,originalCRC,lookBuf,&owned);
 if(result==SZ_OK&&!updateMessage[0])sprintf(updateMessage,"Archive updated: %u entries %s. Remaining file data and supported metadata were verified.",changed,deleting?"deleted":"added or replaced");
 done:
 if(in.file)fclose(in.file);if(sink.output)fclose(sink.output);if(sink.scratch)fclose(sink.scratch);
 if(sink.scratchOwned&&remove(sink.scratchPath))strcpy(updateMessage,"ERROR: A temporary unpacked file could not be removed. Check the archive directory.");
 if(owned&&remove(candidate))strcpy(updateMessage,"ERROR: An incomplete update file could not be removed. The original archive was not replaced.");
 SzArEx_Free(&verify,allocator);SzArEx_Free(&a,allocator);ISzAlloc_Free(allocator,header.data);ISzAlloc_Free(allocator,sink.entries);ISzAlloc_Free(allocator,lookBuf);inputTreeFree(&tree,allocator);return result;
}
#endif
