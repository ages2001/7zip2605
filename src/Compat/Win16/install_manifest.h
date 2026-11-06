/* Size/CRC ownership manifest. Never lists paths outside the install folder. */
#ifndef Z7_WIN16_INSTALL_MANIFEST_H
#define Z7_WIN16_INSTALL_MANIFEST_H
static void installPut32(Byte *p,UInt32 value){unsigned i;for(i=0;i<4;i++){p[i]=(Byte)value;value>>=8;}}
static int writeInstallManifest(const char *directory,const Byte *records,unsigned count){
 char path[260];Byte header[20];FILE *file;int fd,ok;
 sprintf(path,"%s\\INSTALL.DAT",directory);
 fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,S_IRUSR|S_IWUSR);if(fd<0)return 0;
 file=fdopen(fd,"wb");if(!file){close(fd);remove(path);return 0;}
 memcpy(header,"7Z16I33\0",8);installPut32(header+8,count);installPut32(header+12,count*28);
 installPut32(header+16,CrcCalc(records,count*28));
 ok=fwrite(header,1,20,file)==20&&fwrite(records,28,count,file)==count;
 if(fclose(file))ok=0;if(!ok)remove(path);return ok;
}
#endif
