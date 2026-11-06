#include <stdio.h>
#include <string.h>
#include "source_list.h"
int main(void){
 const char *paths[3];char text[1024];int count;
 strcpy(text," \r\n C:\\FIRST.TXT \r\n\"C:\\SECOND.BIN\"\n\tTHIRD.TXT\t\r\n");
 count=splitSourceList(text,paths,3);
 if(count!=3||strcmp(paths[0],"C:\\FIRST.TXT")||strcmp(paths[1],"C:\\SECOND.BIN")||strcmp(paths[2],"THIRD.TXT"))return 1;
 strcpy(text,"ONE\nTWO\nTHREE\nFOUR");if(splitSourceList(text,paths,3)!=-1)return 2;
 strcpy(text,"\"MISSING.TXT");if(splitSourceList(text,paths,3)!=-1)return 3;
 strcpy(text,"\"\"");if(splitSourceList(text,paths,3)!=-1)return 4;
 strcpy(text,"FI\"LE.TXT");if(splitSourceList(text,paths,3)!=-1)return 5;
 memset(text,'A',260);text[260]=0;if(splitSourceList(text,paths,3)!=-1)return 6;
 strcpy(text," \t\r\n");if(splitSourceList(text,paths,3)!=0)return 7;
 puts("PASS multi-source parsing, CR/LF and quotes, capacity and malformed input");return 0;
}
