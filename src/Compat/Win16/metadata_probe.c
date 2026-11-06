/* Native DOS verification of the CLI's extracted timestamp and attribute. */
#include <stdio.h>
#include <sys/stat.h>
#include <dos.h>
int main(void){
 struct stat info;unsigned attributes=0;int valid=1;
 valid&=!stat("C:\\M32OUT\\RONLY.TXT",&info)&&info.st_mtime==946684800UL;
 valid&=!_dos_getfileattr("C:\\M32OUT\\RONLY.TXT",&attributes)&&(attributes&1)!=0;
 valid&=!stat("C:\\M32OUT\\MFILE.TXT",&info)&&info.st_mtime==946684800UL;
 puts(valid?"PASS native extracted modification timestamps and read-only attribute":"FAIL extracted metadata");
 return valid?0:2;
}
