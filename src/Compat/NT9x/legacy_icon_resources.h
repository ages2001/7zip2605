/* Resolve an integer icon-group resource ID to its extraction ordinal without
   loading or executing the image. Win32s' data-file loader is incomplete. */
typedef struct Z7IconFile {HANDLE file;DWORD size;} Z7IconFile;
static WORD iconWord(const BYTE *p){return (WORD)(p[0]|((WORD)p[1]<<8));}
static DWORD iconDword(const BYTE *p){return (DWORD)iconWord(p)|((DWORD)iconWord(p+2)<<16);}
static BOOL iconRead(Z7IconFile *f,DWORD offset,void *buffer,DWORD size){
 DWORD got;LONG high=0;
 if(offset>f->size||size>f->size-offset)return FALSE;
 SetLastError(0);
 if(SetFilePointer(f->file,(LONG)offset,&high,FILE_BEGIN)==INVALID_SET_FILE_POINTER&&GetLastError())return FALSE;
 return ReadFile(f->file,buffer,size,&got,NULL)&&got==size;
}
static BOOL iconResourceRead(Z7IconFile *f,DWORD base,DWORD span,DWORD offset,void *buffer,DWORD size){
 return offset<=span&&size<=span-offset&&base<=f->size&&offset<=f->size-base&&iconRead(f,base+offset,buffer,size);
}
static BOOL iconPeOrdinal(Z7IconFile *f,DWORD pe,UINT id,UINT *ordinal){
 BYTE h[24],optional[144],section[40],directory[16],entry[8];
 DWORD optionalSize,resourceRva,resourceSize,sections,sectionOffset,i,base=0,span=0,groups=0,count,offset;
 if(!iconRead(f,pe,h,sizeof(h))||iconDword(h)!=0x4550)return FALSE;
 sections=iconWord(h+6);optionalSize=iconWord(h+20);
 if(!sections||sections>96||pe>f->size-24||optionalSize>f->size-pe-24)return FALSE;
 if(optionalSize<120||!iconRead(f,pe+24,optional,optionalSize<sizeof(optional)?optionalSize:sizeof(optional)))return FALSE;
 if(iconWord(optional)==0x10b){if(iconDword(optional+92)<3)return FALSE;offset=112;}
 else if(iconWord(optional)==0x20b){if(optionalSize<136||iconDword(optional+108)<3)return FALSE;offset=128;}
 else return FALSE;
 resourceRva=iconDword(optional+offset);resourceSize=iconDword(optional+offset+4);
 if(!resourceRva||resourceSize<16)return FALSE;
 sectionOffset=pe+24+optionalSize;
 if(sections>(f->size-sectionOffset)/40)return FALSE;
 for(i=0;i<sections;i++){
  DWORD address,rawSize,rawOffset,delta;
  if(!iconRead(f,sectionOffset+i*40,section,sizeof(section)))return FALSE;
  address=iconDword(section+12);rawSize=iconDword(section+16);rawOffset=iconDword(section+20);
  if(resourceRva<address)continue;
  delta=resourceRva-address;if(delta>=rawSize)continue;
  if(rawOffset>f->size||rawSize>f->size-rawOffset)return FALSE;
  base=rawOffset+delta;span=rawSize-delta;if(span>resourceSize)span=resourceSize;break;
 }
 if(i==sections||!iconResourceRead(f,base,span,0,directory,16))return FALSE;
 count=(DWORD)iconWord(directory+12)+iconWord(directory+14);
 if(count>(span-16)/8)return FALSE;
 for(i=0;i<count;i++){
  if(!iconResourceRead(f,base,span,16+i*8,entry,8))return FALSE;
  if(iconDword(entry)==14){DWORD value=iconDword(entry+4);if(!(value&0x80000000UL))return FALSE;groups=value&0x7fffffffUL;break;}
 }
 if(i==count||!iconResourceRead(f,base,span,groups,directory,16))return FALSE;
 count=(DWORD)iconWord(directory+12)+iconWord(directory+14);
 if(count>(span-groups-16)/8)return FALSE;
 for(i=0;i<count;i++){
  if(!iconResourceRead(f,base,span,groups+16+i*8,entry,8))return FALSE;
  if(iconDword(entry)==id){if(!(iconDword(entry+4)&0x80000000UL))return FALSE;*ordinal=i;return TRUE;}
 }
 return FALSE;
}
static BOOL iconNeOrdinal(Z7IconFile *f,DWORD ne,UINT id,UINT *ordinal){
 BYTE h[40],type[8],item[12];DWORD offset,end,count,i;WORD resource,names;
 if(id>32767||!iconRead(f,ne,h,sizeof(h))||iconWord(h)!=0x454e)return FALSE;
 resource=iconWord(h+36);names=iconWord(h+38);
 if(!resource||resource>=names||ne>f->size||names>f->size-ne)return FALSE;
 offset=ne+resource;end=ne+names;
 if(end-offset<2||!iconRead(f,offset,item,2)||iconWord(item)>31)return FALSE;
 offset+=2;
 while(offset<end){
  if(end-offset<2||!iconRead(f,offset,type,2))return FALSE;
  if(!iconWord(type))return FALSE;
  if(end-offset<8||!iconRead(f,offset,type,8))return FALSE;
  offset+=8;
  count=iconWord(type+2);if(count>(end-offset)/12)return FALSE;
  if(iconWord(type)==(0x8000|14)){
   for(i=0;i<count;i++){
    if(!iconRead(f,offset+i*12,item,12))return FALSE;
    if(iconWord(item+6)==(0x8000|id)){*ordinal=i;return TRUE;}
   }
   return FALSE;
  }
  offset+=count*12;
 }
 return FALSE;
}
static BOOL iconResourceOrdinal(const char *path,UINT id,UINT *ordinal){
 Z7IconFile f;BYTE dos[64],signature[4];DWORD high=0,offset,error;BOOL ok=FALSE;
 UINT mode=SetErrorMode(SEM_FAILCRITICALERRORS);SetErrorMode(mode|SEM_FAILCRITICALERRORS);
 f.file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
 if(f.file==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_SHARING_VIOLATION)
  f.file=CreateFileA(path,GENERIC_READ,0,NULL,OPEN_EXISTING,0,NULL);
 error=GetLastError();SetErrorMode(mode);
 if(f.file==INVALID_HANDLE_VALUE){SetLastError(error);return FALSE;}
 SetLastError(0);f.size=GetFileSize(f.file,&high);
 if(high||(f.size==INVALID_FILE_SIZE&&GetLastError()))goto done;
 if(!iconRead(&f,0,dos,64)||iconWord(dos)!=0x5a4d)goto done;
 offset=iconDword(dos+60);if(offset<64||!iconRead(&f,offset,signature,4))goto done;
 if(iconDword(signature)==0x4550)ok=iconPeOrdinal(&f,offset,id,ordinal);
 else if(iconWord(signature)==0x454e)ok=iconNeOrdinal(&f,offset,id,ordinal);
 done:CloseHandle(f.file);if(!ok)SetLastError(ERROR_RESOURCE_NAME_NOT_FOUND);return ok;
}
