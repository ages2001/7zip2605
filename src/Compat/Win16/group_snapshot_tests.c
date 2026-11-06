#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "group_snapshot.h"
int main(int argc,char **argv){
 FILE *file;char output[8192];int result;
 if(argc!=3)return 7;file=fopen(argv[1],"rb");if(!file)return 2;
 result=groupSnapshot(file,"7-Zip 26.05 - 16-bit",output,sizeof(output));fclose(file);
 if(result!=atoi(argv[2])){printf("FAIL: expected %s, got %d\n",argv[2],result);return 2;}
 if(result==1){
  if(!strstr(output,"\"7-Zip File Manager\",\"C:\\S33TEST\\7ZFM16.EXE\",")||!strstr(output,"\"Uninstall 7-Zip\",\"C:\\S33TEST\\UNINST16.EXE\","))return 3;
 }
 printf("PASS group snapshot result %d\n",result);return 0;
}
