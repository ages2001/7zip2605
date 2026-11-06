/* Read-only parser for the documented Windows 3.x PMCC group layout.
   Used because Windows 3.0 Program Manager does not answer group requests.
   No group file is edited by this code. */
#ifndef Z7_WIN16_GROUP_SNAPSHOT_H
#define Z7_WIN16_GROUP_SNAPSHOT_H
static unsigned groupWord(const unsigned char *p){return p[0]|((unsigned)p[1]<<8);}
static int groupString(FILE *file,unsigned offset,unsigned limit,char *text,unsigned capacity){
 unsigned n=0;int ch;
 if(!offset||offset>=limit||fseek(file,(long)offset,SEEK_SET))return 0;
 while(offset++<limit){
  ch=fgetc(file);if(ch==EOF)return 0;
  if(!ch){text[n]=0;return 1;}
  if(n+1>=capacity)return 0;text[n++]=(char)ch;
 }
 return 0;
}
/* Returns 1 for a matching group, 0 for a different valid group, -1 for
   malformed input or a snapshot that cannot fit the bounded output buffer. */
static int groupSnapshot(FILE *file,const char *wanted,char *output,unsigned capacity){
 unsigned char header[34],entry[24],word[2];unsigned limit,slots,i,offset,used=0,sum=0,low=0;
 long fileSize,position;char title[260],target[260];int ch;
 if(fseek(file,0,SEEK_END)||(fileSize=ftell(file))<34||fileSize>65535L||fseek(file,0,SEEK_SET))return -1;
 if(fread(header,1,34,file)!=34||memcmp(header,"PMCC",4))return -1;
 limit=groupWord(header+6);slots=groupWord(header+32);
 if(limit<34||limit>fileSize||slots>(limit-34)/2)return -1;
 if(fseek(file,0,SEEK_SET))return -1;
 for(position=0;position<fileSize;position++){
  ch=fgetc(file);if(ch==EOF)return -1;
  if(position&1)sum=(sum+low+((unsigned)ch<<8))&65535U;else low=(unsigned)ch;
 }
 if(fileSize&1)sum=(sum+low)&65535U;if(sum)return -1;
 if(!groupString(file,groupWord(header+22),limit,title,sizeof(title)))return -1;
 if(strcmp(title,wanted))return 0;
 if(strlen(title)+2>=capacity)return -1;strcpy(output,title);strcat(output,"\n");used=(unsigned)strlen(output);
 for(i=0;i<slots;i++){
  if(fseek(file,34L+2L*i,SEEK_SET)||fread(word,1,2,file)!=2)return -1;
  offset=groupWord(word);if(!offset)continue;
  if(offset>limit||limit-offset<24||fseek(file,(long)offset,SEEK_SET)||fread(entry,1,24,file)!=24)return -1;
  if(!groupString(file,groupWord(entry+18),limit,title,sizeof(title))||!groupString(file,groupWord(entry+20),limit,target,sizeof(target)))return -1;
  /* Deliberately prevent ambiguous quoting from establishing ownership. */
  if(strchr(title,'"')||strchr(title,'\r')||strchr(title,'\n'))strcpy(title,"<user item>");
  if(strchr(target,'"')||strchr(target,'\r')||strchr(target,'\n'))strcpy(target,"<user command>");
  if(strlen(title)+strlen(target)+8>=capacity-used)return -1;
  sprintf(output+used,"\"%s\",\"%s\",\n",title,target);used=(unsigned)strlen(output);
 }
 return 1;
}
#endif
