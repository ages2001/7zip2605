/* Transactional classic ZIP updates. Retained records stay byte-for-byte intact.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_UPDATE_ZIP_H
#define Z7_WIN16_UPDATE_ZIP_H
#define ZIP_UPDATE_MAX 256
typedef struct {
 UInt32 central,local,end,centralSize,output;
 int replacement;Byte deleted;char *newName;Byte *newWide;unsigned newNameSize;
} ZipUpdateEntry;
typedef struct {
 ZipUpdateEntry *entries;unsigned count,comment;
 UInt32 central,eocd;
} ZipUpdateDirectory;
typedef struct {FILE *file;Byte *buffer;UInt32 crc;UInt64 position;} ZipUpdateOutput;
static int zuMatch(const Byte *name,const char *text,int subtree){
 unsigned i;for(i=0;text[i];i++){unsigned ch=(unsigned char)text[i];if(ch=='\\')ch='/';if(renameFold(renameChar(name+2*i))!=renameFold(ch))return 0;}
 return !renameChar(name+2*i)||(subtree&&renameChar(name+2*i)=='/');
}
static unsigned zu16(const Byte *b){return b[0]|((unsigned)b[1]<<8);}
static UInt32 zu32(const Byte *b){return (UInt32)zu16(b)|((UInt32)zu16(b+2)<<16);}
static void zuPut(Byte *b,UInt32 n,unsigned bytes){while(bytes--){*b++=(Byte)n;n>>=8;}}
static SRes zuRead(FILE *f,UInt32 offset,void *b,unsigned n){
 if(offset>0x7fffffffUL||fseek(f,(long)offset,SEEK_SET))return SZ_ERROR_READ;
 return fread(b,1,n,f)==n?SZ_OK:SZ_ERROR_READ;
}
/* Called only after the full shared ZIP parser has validated the container. */
static SRes zuDirectory(FILE *f,const CSzArEx *a,ZipUpdateDirectory *d,ISzAllocPtr alloc){
 Byte tail[533],h[46];long length;UInt32 low,high,minimum,cursor,i,next;int found=0;
 if(fseek(f,0,SEEK_END)||(length=ftell(f))<22)return SZ_ERROR_READ;
 high=(UInt32)length-22;minimum=high>65535UL?high-65535UL:0;
 while(!found){unsigned k;low=high-minimum>511?high-511:minimum;
  RINOK(zuRead(f,low,tail,(unsigned)(high-low)+22))k=(unsigned)(high-low)+1;
  while(k--)if(zu32(tail+k)==0x06054b50UL&&low+k+22+zu16(tail+k+20)==(UInt32)length){
   d->eocd=low+k;d->central=zu32(tail+k+16);d->count=zu16(tail+k+10);d->comment=zu16(tail+k+20);found=1;break;
  }
  if(found)break;if(low==minimum)return SZ_ERROR_ARCHIVE;high=low-1;
 }
 if(d->count!=a->NumFiles||d->count>ZIP_UPDATE_MAX)return SZ_ERROR_UNSUPPORTED;
 d->entries=(ZipUpdateEntry *)ISzAlloc_Alloc(alloc,(d->count?d->count:1)*sizeof(ZipUpdateEntry));
 if(!d->entries)return SZ_ERROR_MEM;memset(d->entries,0,(d->count?d->count:1)*sizeof(ZipUpdateEntry));cursor=d->central;
 for(i=0;i<d->count;i++){
  ZipUpdateEntry *e=d->entries+i;UInt64 end=a->db.PackPositions[2*i+1];
  RINOK(zuRead(f,cursor,h,46))e->central=cursor;e->centralSize=46UL+zu16(h+28)+zu16(h+30)+zu16(h+32);e->local=zu32(h+42);e->replacement=-1;
  if(zu16(h+8)&8){Byte desc[12];RINOK(zuRead(f,(UInt32)end,desc,12))
   end+=(zu32(desc)==zu32(h+16)&&zu32(desc+4)==zu32(h+20)&&zu32(desc+8)==zu32(h+24))?12:16;
  }
  if(end>d->central)return SZ_ERROR_ARCHIVE;e->end=(UInt32)end;cursor+=e->centralSize;
 }
 if(cursor!=d->eocd)return SZ_ERROR_ARCHIVE;
 /* Do not silently discard executable prefixes or unrecognized gap records. */
 cursor=0;
 for(i=0;i<d->count;i++){unsigned j;next=cursor;
  for(j=0;j<d->count;j++)if(d->entries[j].local==cursor){next=d->entries[j].end;break;}
  if(next==cursor)return SZ_ERROR_UNSUPPORTED;cursor=next;
 }
 return cursor==d->central?SZ_OK:SZ_ERROR_UNSUPPORTED;
}
static SRes zuWrite(ZipUpdateOutput *out,const void *data,unsigned n){
 if(out->position+n>0x7fffffffUL)return SZ_ERROR_UNSUPPORTED;
 if(testProgress&&!testProgress(out->position))return SZ_ERROR_PROGRESS;
 if(!createWriteBytes(out->file,data,n))return SZ_ERROR_WRITE;
 out->crc=CrcUpdate(out->crc,data,n);out->position+=n;return SZ_OK;
}
static SRes zuCopy(ZipUpdateOutput *out,FILE *input,UInt32 offset,UInt32 size){
 while(size){unsigned n=size>4096?4096:(unsigned)size;RINOK(zuRead(input,offset,out->buffer,n))RINOK(zuWrite(out,out->buffer,n))offset+=n;size-=n;}return SZ_OK;
}
/* Unknown replacement extras could depend on file contents. Reject those
   replacements; for retained entries every opaque byte is copied unchanged. */
static SRes zuExtras(FILE *input,UInt32 offset,unsigned size,ZipUpdateOutput *out,UInt32 mtime){
 while(size){Byte extra[9];unsigned n;
  if(size<4)return SZ_ERROR_ARCHIVE;RINOK(zuRead(input,offset,extra,4))n=zu16(extra+2);
  if(n>size-4)return SZ_ERROR_ARCHIVE;
  if(zu16(extra)!=0x5455||n!=5)return SZ_ERROR_UNSUPPORTED;
  RINOK(zuRead(input,offset+4,extra+4,5))if(extra[4]!=1)return SZ_ERROR_UNSUPPORTED;
  if(out){zuPut(extra+5,mtime,4);RINOK(zuWrite(out,extra,9))}
  offset+=9;size-=9;
 }
 return SZ_OK;
}
static SRes zuCheckReplacement(FILE *file,const ZipUpdateEntry *e){
 Byte h[46],l[30];RINOK(zuRead(file,e->central,h,46))RINOK(zuRead(file,e->local,l,30))
 RINOK(zuExtras(file,e->central+46+zu16(h+28),zu16(h+30),NULL,0))
 return zuExtras(file,e->local+30+zu16(l+26),zu16(l+28),NULL,0);
}
static SRes zuReplace(ZipUpdateOutput *out,FILE *oldFile,const ZipUpdateEntry *old,FILE *newFile,const ZipUpdateEntry *fresh,int central){
 Byte h[46],n[46],l[30],nl[30],stamp[9];UInt32 time,newData;unsigned name,extra;
 RINOK(zuRead(oldFile,old->central,h,46))RINOK(zuRead(newFile,fresh->central,n,46))
 RINOK(zuRead(newFile,fresh->central+46+zu16(n+28),stamp,9))
 if(zu16(stamp)!=0x5455||zu16(stamp+2)!=5||stamp[4]!=1)return SZ_ERROR_ARCHIVE;time=zu32(stamp+5);
 name=zu16(h+28);
 if(central){
  extra=zu16(h+30);memcpy(h+6,n+6,2);zuPut(h+8,zu16(h+8)&0x800,2);memcpy(h+10,n+10,18);
  zuPut(h+38,(zu32(h+38)&0xffff0000UL)|(zu32(n+38)&0xffffUL),4);zuPut(h+42,old->output,4);
  RINOK(zuWrite(out,h,46))RINOK(zuCopy(out,oldFile,old->central+46,name))
  RINOK(zuExtras(oldFile,old->central+46+name,extra,out,time))
  return zuCopy(out,oldFile,old->central+46+name+extra,zu16(h+32));
 }
 RINOK(zuRead(oldFile,old->local,l,30))RINOK(zuRead(newFile,fresh->local,nl,30))
 newData=fresh->local+30+zu16(nl+26)+zu16(nl+28);extra=zu16(l+28);
 zuPut(nl+6,zu16(h+8)&0x800,2);zuPut(nl+26,name,2);zuPut(nl+28,extra,2);
 RINOK(zuWrite(out,nl,30))RINOK(zuCopy(out,oldFile,old->central+46,name))
 RINOK(zuExtras(oldFile,old->local+30+zu16(l+26),extra,out,time))
 return zuCopy(out,newFile,newData,zu32(n+20));
}
static SRes zuRename(ZipUpdateOutput *out,FILE *input,const ZipUpdateEntry *e,int central){
 Byte h[46];unsigned fixed=central?46:30,n;
 RINOK(zuRead(input,central?e->central:e->local,h,fixed))n=zu16(h+(central?28:26));
 zuPut(h+(central?28:26),e->newNameSize,2);if(central)zuPut(h+42,e->output,4);
 RINOK(zuWrite(out,h,fixed))RINOK(zuWrite(out,e->newName,e->newNameSize))
 return central?zuCopy(out,input,e->central+46+n,e->centralSize-46-n):zuCopy(out,input,e->local+30+n,e->end-e->local-30-n);
}
static SRes zuRenamePlan(CSzArEx *a,ZipUpdateDirectory *d,FILE *input,const char *oldName,const char *newName,ISzAllocPtr alloc,unsigned *changed){
 unsigned i,j,oldLength=(unsigned)strlen(oldName),newLength=(unsigned)strlen(newName);
 if(!renamePath(oldName)||!renamePath(newName))return SZ_ERROR_UNSUPPORTED;
 for(i=0;i<d->count;i++){
  ZipUpdateEntry *e=d->entries+i;const Byte *wide=a->FileNames+2*a->FileNameOffsets[i];unsigned units,n,k;Byte h[46];
  if(!zuMatch(wide,oldName,1))continue;
  RINOK(zuCheckReplacement(input,e))
  units=(unsigned)(a->FileNameOffsets[i+1]-a->FileNameOffsets[i]);
  if(units<oldLength+1||units-oldLength+newLength>260)return SZ_ERROR_UNSUPPORTED;
  RINOK(zuRead(input,e->central,h,46))n=zu16(h+28);
  if(n<oldLength||(UInt32)n-oldLength+newLength>65520UL)return SZ_ERROR_UNSUPPORTED;
  e->newNameSize=n-oldLength+newLength;
  e->newName=(char *)ISzAlloc_Alloc(alloc,e->newNameSize);
  e->newWide=(Byte *)ISzAlloc_Alloc(alloc,2*(units-oldLength+newLength));
  if(!e->newName||!e->newWide)return SZ_ERROR_MEM;
  for(k=0;k<newLength;k++){unsigned ch=(unsigned char)newName[k];if(ch=='\\')ch='/';e->newName[k]=(char)ch;e->newWide[2*k]=(Byte)ch;e->newWide[2*k+1]=0;}
  RINOK(zuRead(input,e->central+46+oldLength,e->newName+newLength,n-oldLength))
  memcpy(e->newWide+2*newLength,wide+2*oldLength,2*(units-oldLength));(*changed)++;
 }
 for(i=0;i<d->count;i++)for(j=0;j<i;j++){
  const Byte *left=d->entries[i].newWide?d->entries[i].newWide:a->FileNames+2*a->FileNameOffsets[i];
  const Byte *right=d->entries[j].newWide?d->entries[j].newWide:a->FileNames+2*a->FileNameOffsets[j];
  if(renameCollision(left,right,SzArEx_IsDir(a,i),SzArEx_IsDir(a,j)))return SZ_ERROR_UNSUPPORTED;
 }
 return SZ_OK;
}
static SRes zuCentral(ZipUpdateOutput *out,FILE *input,const ZipUpdateEntry *e){
 Byte h[46];RINOK(zuRead(input,e->central,h,46))zuPut(h+42,e->output,4);
 RINOK(zuWrite(out,h,46))return zuCopy(out,input,e->central+46,e->centralSize-46);
}
static SRes zuOpen(Input *in,CLookToRead2 *look,CSzArEx *a,ISzAllocPtr alloc){
 LookToRead2_INIT(look);in->extraMetadata=0;
 return Z7Win16_OpenZip(a,&in->iface,alloc,&in->extraMetadata);
}
static SRes updateZip(const char *path,unsigned count,const char * const *sources,ISzAllocPtr alloc,int deflate,int deleting){
 Input input,added;CLookToRead2 look;CSzArEx a,b,verify;ZipUpdateDirectory old,fresh;ZipUpdateOutput out;
 Byte *buffer=NULL;char candidate[260],scratch[260];unsigned i,j,attempt,changed=0,total=0;int owned=0,scratchOwned=0,fd;
 InputTree tree;SRes result=SZ_OK;struct stat original,st;UInt32 originalCRC,checkCRC,central;const char *name;
 memset(&tree,0,sizeof(tree));memset(&old,0,sizeof(old));memset(&fresh,0,sizeof(fresh));memset(&out,0,sizeof(out));memset(&input,0,sizeof(input));memset(&added,0,sizeof(added));
 SzArEx_Init(&a);SzArEx_Init(&b);SzArEx_Init(&verify);updateMessage[0]=0;candidate[0]=scratch[0]=0;
 if(!count||count>ZIP_UPDATE_MAX)return SZ_ERROR_UNSUPPORTED;
#ifdef __WATCOMC__
 if(!validDosDestination(path))return SZ_ERROR_UNSUPPORTED;input.file=_fsopen(path,"rb",SH_DENYWR);
#else
 input.file=fopen(path,"rb");
#endif
 if(!input.file||stat(path,&original)){result=SZ_ERROR_READ;goto done;}
 if(!(original.st_mode&S_IWUSR)){result=SZ_ERROR_WRITE;goto done;}
 buffer=(Byte *)ISzAlloc_Alloc(alloc,4096);if(!buffer){result=SZ_ERROR_MEM;goto done;}
 input.iface.Read=added.iface.Read=readInput;input.iface.Seek=added.iface.Seek=seekInput;
 /* CRC/copy and archive decoding are sequential, so share the I/O buffer. */
 LookToRead2_CreateVTable(&look,False);look.realStream=&input.iface;look.buf=buffer;look.bufSize=4096;
 result=updateFileCRC(input.file,buffer,&originalCRC);if(result!=SZ_OK)goto done;
 result=zuOpen(&input,&look,&a,alloc);if(result!=SZ_OK)goto done;
 result=zuDirectory(input.file,&a,&old,alloc);if(result!=SZ_OK)goto done;
 /* Ambiguous duplicate names and file/directory collisions are not updated. */
 for(i=0;i<a.NumFiles;i++)for(j=0;j<i;j++)if(renameCollision(a.FileNames+2*a.FileNameOffsets[i],a.FileNames+2*a.FileNameOffsets[j],SzArEx_IsDir(&a,i),SzArEx_IsDir(&a,j))){result=SZ_ERROR_UNSUPPORTED;goto done;}
 if(deleting==2){
  if(count!=2){result=SZ_ERROR_UNSUPPORTED;goto done;}
  result=zuRenamePlan(&a,&old,input.file,sources[0],sources[1],alloc,&changed);
  if(result!=SZ_OK){strcpy(updateMessage,"ERROR: Rename would create a name collision or invalidate unsupported ZIP metadata. The original archive was not changed.");goto done;}
 }
 if(!deleting){result=inputTreeBuild(&tree,count,sources,path,alloc);if(result!=SZ_OK){strcpy(updateMessage,inputTreeMessage);goto done;}count=tree.count;}
 for(j=0;deleting!=2&&j<count;j++){
  name=deleting?sources[j]:tree.items[j].name;
  if(deleting){
   if(!renamePath(name)){result=SZ_ERROR_UNSUPPORTED;goto done;}
   for(i=0;i<old.count;i++)if(zuMatch(a.FileNames+2*a.FileNameOffsets[i],name,1)&&!old.entries[i].deleted){old.entries[i].deleted=1;changed++;}
  }else{
   Byte wide[520];unsigned k;
   for(k=0;k<=strlen(name);k++){wide[2*k]=(Byte)name[k];wide[2*k+1]=0;}
   for(i=0;i<old.count;i++)if(renameCollision(a.FileNames+2*a.FileNameOffsets[i],wide,SzArEx_IsDir(&a,i),tree.items[j].directory)){
    if(!!SzArEx_IsDir(&a,i)!=!!tree.items[j].directory||!zuMatch(a.FileNames+2*a.FileNameOffsets[i],name,0)||old.entries[i].replacement>=0){result=SZ_ERROR_UNSUPPORTED;goto done;}
    old.entries[i].replacement=(int)j;result=zuCheckReplacement(input.file,old.entries+i);
    if(result!=SZ_OK){strcpy(updateMessage,"ERROR: Replacement would discard or invalidate unsupported ZIP metadata. The original archive was not changed.");goto done;}break;
   }
   changed++;
  }
 }
 if(!changed){strcpy(updateMessage,"No matching archive entries. The archive is unchanged.");goto done;}
 result=testArchive(&a,&input,&look,alloc);if(result!=SZ_OK)goto done;
 /* The update plan owns offsets and copied names. No later phase needs the
    old parsed archive; release it before allocating the encoder window. */
 SzArEx_Free(&a,alloc);
 if(!deleting){
  for(attempt=0;attempt<4096;attempt++){if(!renameSibling(path,scratch,attempt,"ZIP")){result=SZ_ERROR_UNSUPPORTED;goto done;}if(stat(scratch,&st))break;}
  if(attempt==4096){result=SZ_ERROR_WRITE;goto done;}
  result=createZipTree(scratch,&tree,alloc,deflate);if(result!=SZ_OK)goto done;scratchOwned=1;
  added.file=fopen(scratch,"rb");if(!added.file){result=SZ_ERROR_READ;goto done;}
  look.realStream=&added.iface;result=zuOpen(&added,&look,&b,alloc);if(result!=SZ_OK)goto done;
  result=zuDirectory(added.file,&b,&fresh,alloc);if(result!=SZ_OK)goto done;
  SzArEx_Free(&b,alloc);
  inputTreeFree(&tree,alloc);
  for(i=0;i<old.count;i++)if(old.entries[i].replacement>=0)fresh.entries[old.entries[i].replacement].deleted=1;
 }
 for(i=0;i<old.count;i++)if(!old.entries[i].deleted)total++;
 for(i=0;i<fresh.count;i++)if(!fresh.entries[i].deleted)total++;
 if(total>ZIP_UPDATE_MAX){result=SZ_ERROR_UNSUPPORTED;goto done;}
 for(attempt=0;attempt<4096;attempt++){if(!renameSibling(path,candidate,attempt,"TMP")){result=SZ_ERROR_UNSUPPORTED;goto done;}fd=open(candidate,O_RDWR|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);if(fd>=0)break;}
 if(attempt==4096){result=SZ_ERROR_WRITE;goto done;}owned=1;out.file=fdopen(fd,"w+b");if(!out.file){close(fd);result=SZ_ERROR_WRITE;goto done;}
 out.buffer=buffer;out.crc=CRC_INIT_VAL;
 for(i=0;i<old.count;i++)if(!old.entries[i].deleted){ZipUpdateEntry *e=old.entries+i;e->output=(UInt32)out.position;
  result=e->newName?zuRename(&out,input.file,e,0):e->replacement<0?zuCopy(&out,input.file,e->local,e->end-e->local):zuReplace(&out,input.file,e,added.file,fresh.entries+e->replacement,0);
  if(result!=SZ_OK)goto done;
 }
 for(i=0;i<fresh.count;i++)if(!fresh.entries[i].deleted){ZipUpdateEntry *e=fresh.entries+i;e->output=(UInt32)out.position;result=zuCopy(&out,added.file,e->local,e->end-e->local);if(result!=SZ_OK)goto done;}
 central=(UInt32)out.position;
 for(i=0;i<old.count;i++)if(!old.entries[i].deleted){ZipUpdateEntry *e=old.entries+i;
  result=e->newName?zuRename(&out,input.file,e,1):e->replacement<0?zuCentral(&out,input.file,e):zuReplace(&out,input.file,e,added.file,fresh.entries+e->replacement,1);if(result!=SZ_OK)goto done;
 }
 for(i=0;i<fresh.count;i++)if(!fresh.entries[i].deleted){result=zuCentral(&out,added.file,fresh.entries+i);if(result!=SZ_OK)goto done;}
 {Byte end[22];memset(end,0,22);zuPut(end,0x06054b50UL,4);zuPut(end+8,total,2);zuPut(end+10,total,2);zuPut(end+12,(UInt32)out.position-central,4);zuPut(end+16,central,4);zuPut(end+20,old.comment,2);
  result=zuWrite(&out,end,22);if(result!=SZ_OK)goto done;result=zuCopy(&out,input.file,old.eocd+22,old.comment);if(result!=SZ_OK)goto done;
 }
 if(fclose(out.file)){out.file=NULL;result=SZ_ERROR_WRITE;goto done;}out.file=NULL;
 fclose(input.file);input.file=fopen(candidate,"rb");if(!input.file){result=SZ_ERROR_READ;goto done;}
 result=updateFileCRC(input.file,buffer,&checkCRC);if(result==SZ_OK&&checkCRC!=out.crc)result=SZ_ERROR_CRC;if(result!=SZ_OK)goto done;
 look.realStream=&input.iface;result=zuOpen(&input,&look,&verify,alloc);if(result!=SZ_OK)goto done;
 if(verify.NumFiles!=total){result=SZ_ERROR_ARCHIVE;goto done;}
 result=testArchive(&verify,&input,&look,alloc);if(result!=SZ_OK)goto done;
 fclose(input.file);input.file=NULL;
 if(added.file){fclose(added.file);added.file=NULL;}
 if(scratchOwned){if(remove(scratch)){result=SZ_ERROR_WRITE;goto done;}scratchOwned=0;}
 result=updateCommit(path,candidate,&original,originalCRC,buffer,&owned);
 if(result==SZ_OK&&!updateMessage[0])sprintf(updateMessage,"ZIP archive updated and verified: %u entries %s. Retained data and metadata are preserved.",changed,deleting==2?"renamed":deleting?"deleted":"added or replaced");
 done:
 if(input.file)fclose(input.file);if(added.file)fclose(added.file);if(out.file)fclose(out.file);
 if(scratchOwned&&remove(scratch))strcpy(updateMessage,"ERROR: A temporary ZIP could not be removed. Check the archive directory.");
 if(owned&&remove(candidate))strcpy(updateMessage,"ERROR: An incomplete ZIP update could not be removed. The original archive was not replaced.");
 if(old.entries)for(i=0;i<old.count;i++){ISzAlloc_Free(alloc,old.entries[i].newName);ISzAlloc_Free(alloc,old.entries[i].newWide);}
 SzArEx_Free(&verify,alloc);SzArEx_Free(&b,alloc);SzArEx_Free(&a,alloc);ISzAlloc_Free(alloc,old.entries);ISzAlloc_Free(alloc,fresh.entries);ISzAlloc_Free(alloc,buffer);inputTreeFree(&tree,alloc);return result;
}
/* Detect content, not filename extensions, for existing archive operations. */
static int archiveIsZip(const char *path){FILE *f=fopen(path,"rb");Byte h[4];int zip=0;if(f){zip=fread(h,1,4,f)==4&&(zu32(h)==0x04034b50UL||zu32(h)==0x06054b50UL);fclose(f);}return zip;}
static SRes renameAnyArchive(const char *path,const char *oldName,const char *newName,ISzAllocPtr alloc){
 const char *names[2];SRes result;if(!archiveIsZip(path))return renameArchive(path,oldName,newName,alloc);
 names[0]=oldName;names[1]=newName;result=updateZip(path,2,names,alloc,0,2);strcpy(renameMessage,updateMessage);return result;
}
static SRes updateAnyArchive(const char *path,unsigned count,const char * const *sources,ISzAllocPtr alloc,int compressed,int deleting){
 return archiveIsZip(path)?updateZip(path,count,sources,alloc,compressed,deleting):updateArchive(path,count,sources,alloc,compressed,deleting);
}
#endif
