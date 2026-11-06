/* Checked entry masks shared by the native GUI and CLI.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_ARCHIVE_SELECTION_H
#define Z7_WIN16_ARCHIVE_SELECTION_H
static unsigned selectionChar(unsigned c){return c=='\\'?'/':foldName(c);}
static unsigned selectionEntryChar(const CSzArEx *a,size_t offset){return a->FileNames[2*offset]|((unsigned)a->FileNames[2*offset+1]<<8);}
static SRes archiveSelectionAllocate(const CSzArEx *a,ISzAllocPtr alloc,Byte **mask,size_t *bytes){
 UInt32 required=a->NumFiles/8+(a->NumFiles%8!=0);*mask=NULL;*bytes=0;
 if((UInt64)required>(size_t)-1)return SZ_ERROR_MEM;
 *bytes=(size_t)required;*mask=(Byte *)ISzAlloc_Alloc(alloc,*bytes?*bytes:1);
 if(!*mask)return SZ_ERROR_MEM;memset(*mask,0,*bytes?*bytes:1);return SZ_OK;
}
static int archiveEntryBelow(const CSzArEx *a,UInt32 parent,UInt32 child){
 size_t p=a->FileNameOffsets[parent],end=a->FileNameOffsets[parent+1]-1;
 size_t q=a->FileNameOffsets[child],qend=a->FileNameOffsets[child+1]-1;
 while(end>p&&selectionChar(selectionEntryChar(a,end-1))=='/')end--;
 if(end==p)return 0;
 for(;p<end;p++,q++)if(q>=qend||selectionChar(selectionEntryChar(a,p))!=selectionChar(selectionEntryChar(a,q)))return 0;
 return q<qend&&selectionChar(selectionEntryChar(a,q))=='/';
}
static SRes archiveSelectionAdd(const CSzArEx *a,Byte *mask,UInt32 index){
 UInt32 i;if(!mask||index>=a->NumFiles)return SZ_ERROR_PARAM;mask[index/8]|=1u<<(index%8);
 if(SzArEx_IsDir(a,index))for(i=0;i<a->NumFiles;i++)if(archiveEntryBelow(a,index,i))mask[i/8]|=1u<<(i%8);
 return SZ_OK;
}
/* Literal archive paths include descendants at a separator boundary. The
   extraction layer independently validates every selected destination path. */
static int archiveEntryMatches(const CSzArEx *a,UInt32 index,const char *name,size_t length){
 size_t p=a->FileNameOffsets[index],end=a->FileNameOffsets[index+1]-1,i;
 for(i=0;i<length;i++,p++)if(p>=end||selectionChar(selectionEntryChar(a,p))!=selectionChar((unsigned char)name[i]))return 0;
 return p==end||selectionChar(selectionEntryChar(a,p))=='/';
}
/* Iterative glob: '*' matches any run of characters including separators, '?'
   matches one character. No recursion: the 16-bit stack is small. Comparison
   uses the shared case/separator folding so DOS 8.3 names match regardless of
   case or slash direction. The pattern is matched against the full entry path.
   patternLength excludes any trailing separators (stripped by the caller). */
static int archiveGlobMatch(const CSzArEx *a,UInt32 index,const char *pattern,size_t patternLength){
 size_t nameStart=a->FileNameOffsets[index],nameEnd=a->FileNameOffsets[index+1]-1;
 size_t p=0,s=nameStart;              /* current pattern / name positions   */
 size_t star=(size_t)-1,sStar=0;      /* last '*' pattern pos and its name pos */
 while(nameEnd>nameStart&&selectionChar(selectionEntryChar(a,nameEnd-1))=='/')nameEnd--;
 /* Greedy two-pointer wildcard match: consume the name, remembering the most
    recent '*' so a dead end backtracks to it and lets the star eat one more
    character. Terminates when the whole name is consumed. */
 while(s<nameEnd){
  if(p<patternLength&&pattern[p]!='*'&&
     (pattern[p]=='?'||selectionChar((unsigned char)pattern[p])==selectionChar(selectionEntryChar(a,s)))){p++;s++;}
  else if(p<patternLength&&pattern[p]=='*'){star=p++;sStar=s;}
  else if(star!=(size_t)-1){p=star+1;s=++sStar;}
  else return 0;
 }
 while(p<patternLength&&pattern[p]=='*')p++;
 return p==patternLength;
}
static int selectorHasWildcard(const char *name,size_t length){
 size_t j;for(j=0;j<length;j++)if(name[j]=='*'||name[j]=='?')return 1;return 0;
}
static SRes archiveSelectionNames(const CSzArEx *a,unsigned count,const char *const *names,ISzAllocPtr alloc,Byte **mask,size_t *bytes){
 unsigned n;UInt32 i;SRes result;
 *mask=NULL;*bytes=0;if(!count)return SZ_ERROR_PARAM;
 result=archiveSelectionAllocate(a,alloc,mask,bytes);if(result!=SZ_OK)return result;
 for(n=0;n<count;n++){
  size_t length=strlen(names[n]),j;int found=0,wild;
  while(length&&selectionChar((unsigned char)names[n][length-1])=='/')length--;
  if(!length){result=SZ_ERROR_PARAM;goto failed;}
  for(j=0;j<length;j++)if((unsigned char)names[n][j]>=127){result=SZ_ERROR_UNSUPPORTED;goto failed;}
  wild=selectorHasWildcard(names[n],length);
  for(i=0;i<a->NumFiles;i++){
   int hit=wild?archiveGlobMatch(a,i,names[n],length):archiveEntryMatches(a,i,names[n],length);
   if(hit){archiveSelectionAdd(a,*mask,i);found=1;}
  }
  if(!found){result=SZ_ERROR_PARAM;goto failed;}
 }
 return SZ_OK;
failed:
 ISzAlloc_Free(alloc,*mask);*mask=NULL;*bytes=0;return result;
}
#endif
