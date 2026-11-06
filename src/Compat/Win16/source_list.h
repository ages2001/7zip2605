/* Split the GUI's one-path-per-line input without shell wildcard expansion.
   The caller owns the mutable buffer throughout archive creation. */
#ifndef Z7_WIN16_SOURCE_LIST_H
#define Z7_WIN16_SOURCE_LIST_H
static int splitSourceList(char *text,const char **paths,unsigned capacity){
 unsigned count=0;char *line,*end;
 while(*text){
  line=text;while(*text&&*text!='\r'&&*text!='\n')text++;
  if(*text)*text++=0;
  while(*line==' '||*line=='\t')line++;
  end=line+strlen(line);while(end>line&&(end[-1]==' '||end[-1]=='\t'))*--end=0;
  if(!*line)continue;
  if(*line=='"'){
   if(end-line<2||end[-1]!='"')return -1;
   line++;*--end=0;if(!*line)return -1;
  }
  if(strchr(line,'"')||end-line>259||count==capacity)return -1;
  paths[count++]=line;
 }
 return (int)count;
}
#endif
