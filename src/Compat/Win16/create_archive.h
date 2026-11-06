/* Streaming 7z writer for native DOS/Win16. Sources are never modified.
   Source trees use DOS 8.3 components and retain empty directories. */
#ifndef Z7_WIN16_CREATE_ARCHIVE_H
#define Z7_WIN16_CREATE_ARCHIVE_H
#include <fcntl.h>
#include <time.h>
#ifdef __WATCOMC__
#include <io.h>
#else
#include <unistd.h>
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#define CREATE_HEADER_CAPACITY 32760
#include "create_lzma.h"
#include "input_tree.h"
#ifdef Z7_WIN16_WRITE_XZ_AES
#include "aes7z_enc.h"
#endif
static char createMessage[320];
typedef struct { const char *name; Byte directory,properties[5]; UInt64 size,packed,mtime,lzmaUnpack; UInt32 crc,attributes; Byte iv[16]; } CreateEntry;
#define AES7Z_TEMP "7ZENCTMP.TMP"
typedef struct { Byte *data; unsigned used; int failed; } CreateHeader;
static void createByte(CreateHeader *h,unsigned value){
 if(h->used==CREATE_HEADER_CAPACITY){h->failed=1;return;}h->data[h->used++]=(Byte)value;
}
static void createUInt(CreateHeader *h,UInt64 value){
 unsigned i,j,first=0,mask=0x80;
 for(i=0;i<8;i++){
  if(value<((UInt64)1<<(7*(i+1)))){first|=(unsigned)(value>>(8*i));break;}
  first|=mask;mask>>=1;
 }
 createByte(h,first);for(j=0;j<i;j++){createByte(h,(unsigned)value);value>>=8;}
}
static void createLE(CreateHeader *h,UInt64 value,unsigned bytes){while(bytes--){createByte(h,(unsigned)value);value>>=8;}}
static int createWriteBytes(FILE *file,const void *data,size_t size){return fwrite(data,1,size,file)==size;}
static SRes createArchiveMethod(const char *destination,unsigned count,const char * const *sources,ISzAllocPtr allocator,int lzma){
 CreateEntry *entries=NULL;CreateHeader h;Byte *buffer=NULL;FILE *output=NULL,*input=NULL;
 unsigned i,j,streams=0,empty=0;UInt64 packed=0,processed=0;SRes result=SZ_OK;int owned=0,fd;
 Byte signature[32];CreateHeader start;struct stat st;InputTree tree;
#ifdef Z7_WIN16_WRITE_XZ_AES
 int encrypt=lzma&&aes7zEncHasPassword();
#else
 int encrypt=0;
#endif
 createMessage[0]=0;memset(&h,0,sizeof(h));memset(&tree,0,sizeof(tree));
#ifdef Z7_WIN16_WRITE_XZ_AES
 if(encrypt)aes7zRngSeed();
#endif

#ifdef __WATCOMC__
 if(!validDosDestination(destination)){strcpy(createMessage,"ERROR: Archive path must use valid DOS 8.3 names.");return SZ_ERROR_UNSUPPORTED;}
#endif
 result=inputTreeBuild(&tree,count,sources,destination,allocator);if(result!=SZ_OK){strcpy(createMessage,inputTreeMessage);goto done;}count=tree.count;
 entries=(CreateEntry *)ISzAlloc_Alloc(allocator,count*sizeof(CreateEntry));
 if(!lzma)buffer=(Byte *)ISzAlloc_Alloc(allocator,4096);
 if(!entries||(!lzma&&!buffer)){result=SZ_ERROR_MEM;goto done;}
 for(i=0;i<count;i++){
  const InputNode *node=tree.items+i;
  entries[i].name=node->name;entries[i].directory=node->directory;
  if(stat(node->source,&st)){result=SZ_ERROR_READ;goto done;}
  if((st.st_mode&S_IFMT)!=(node->directory?S_IFDIR:S_IFREG)||st.st_size<0){result=SZ_ERROR_READ;goto done;}
  entries[i].size=node->directory?0:(UInt64)st.st_size;entries[i].packed=0;
  entries[i].mtime=((UInt64)((Int64)st.st_mtime+11644473600LL))*10000000UL;
  entries[i].attributes=(node->directory?0x10:0x20)|((st.st_mode&S_IWUSR)?0:1);
#ifdef __WATCOMC__
  {unsigned attr;if(_dos_getfileattr(node->source,&attr)){result=SZ_ERROR_READ;goto done;}entries[i].attributes=attr;}
#endif
  if(entries[i].size)streams++;else empty++;
 }
 if(testProgress&&!testProgress(0)){result=SZ_ERROR_PROGRESS;goto done;}
 fd=open(destination,O_WRONLY|O_CREAT|O_EXCL|O_BINARY, S_IRUSR|S_IWUSR);
 if(fd<0){strcpy(createMessage,"ERROR: Cannot create a new archive. Existing files are never overwritten.");result=SZ_ERROR_WRITE;goto done;}
 owned=1;output=fdopen(fd,"wb");if(!output){close(fd);result=SZ_ERROR_WRITE;goto done;}
 memset(signature,0,sizeof(signature));if(!createWriteBytes(output,signature,sizeof(signature))){result=SZ_ERROR_WRITE;goto done;}
 for(i=0;i<count;i++){
  UInt64 remaining=entries[i].size;UInt32 crc=CRC_INIT_VAL;
  if(entries[i].directory)continue;
  input=fopen(tree.items[i].source,"rb");if(!input){result=SZ_ERROR_READ;goto done;}
  if(lzma&&remaining){
#ifdef Z7_WIN16_WRITE_XZ_AES
   if(encrypt){
    FILE *tmp=fopen(AES7Z_TEMP,"wb+");
    if(!tmp){result=SZ_ERROR_WRITE;goto done;}
    result=createLzmaStream(input,tmp,remaining,processed,&entries[i].lzmaUnpack,&entries[i].crc,entries[i].properties,allocator);
    if(result==SZ_OK)result=aes7zEncryptStream(tmp,entries[i].lzmaUnpack,output,AES7Z_ENC_NCP,entries[i].iv,&entries[i].packed,allocator);
    fclose(tmp);remove(AES7Z_TEMP);
    if(result!=SZ_OK)goto done;
    processed+=remaining;packed+=entries[i].packed;
   }else
#endif
   {
    result=createLzmaStream(input,output,remaining,processed,&entries[i].packed,&entries[i].crc,entries[i].properties,allocator);
    if(result!=SZ_OK)goto done;
    processed+=remaining;packed+=entries[i].packed;
   }
   if(fclose(input)){input=NULL;result=SZ_ERROR_READ;goto done;}input=NULL;continue;
  }
  while(remaining){
   size_t n=remaining>4096?4096:(size_t)remaining;
   if(testProgress&&!testProgress(processed)){result=SZ_ERROR_PROGRESS;goto done;}
   if(fread(buffer,1,n,input)!=n){result=SZ_ERROR_READ;goto done;}
   if(!createWriteBytes(output,buffer,n)){result=SZ_ERROR_WRITE;goto done;}
   crc=CrcUpdate(crc,buffer,n);processed+=n;remaining-=n;
  }
  if(fgetc(input)!=EOF||ferror(input)){result=SZ_ERROR_READ;goto done;}
  if(fclose(input)){input=NULL;result=SZ_ERROR_READ;goto done;}input=NULL;entries[i].crc=CRC_GET_DIGEST(crc);
  entries[i].packed=entries[i].size;packed+=entries[i].packed;
 }
 if(testProgress&&!testProgress(processed)){result=SZ_ERROR_PROGRESS;goto done;}
 /* Header memory is not needed while the encoder owns its working buffers. */
 h.data=(Byte *)ISzAlloc_Alloc(allocator,CREATE_HEADER_CAPACITY);if(!h.data){result=SZ_ERROR_MEM;goto done;}
 createByte(&h,1); /* Header */
 if(streams){
  createByte(&h,4);createByte(&h,6);createUInt(&h,0);createUInt(&h,streams);
  createByte(&h,9);for(i=0;i<count;i++)if(entries[i].size)createUInt(&h,entries[i].packed);
  createByte(&h,0);createByte(&h,7);createByte(&h,11);createUInt(&h,streams);createByte(&h,0);
  for(i=0;i<count;i++)if(entries[i].size){
   if(encrypt){
    /* 2-coder folder: 7zAES (pack-fed) -> LZMA; bond InIndex=1 (LZMA in),
       OutIndex=0 (AES coder); props = ncp19,saltSize0,iv16. */
    createUInt(&h,2);
    createByte(&h,0x24);createByte(&h,0x06);createByte(&h,0xF1);createByte(&h,0x07);createByte(&h,0x01);
    createUInt(&h,18);createByte(&h,0x53);createByte(&h,0x0F);for(j=0;j<16;j++)createByte(&h,entries[i].iv[j]);
    createByte(&h,0x23);createByte(&h,3);createByte(&h,1);createByte(&h,1);createUInt(&h,5);for(j=0;j<5;j++)createByte(&h,entries[i].properties[j]);
    createUInt(&h,1);createUInt(&h,0);
   }else{
    createUInt(&h,1);
    if(lzma){createByte(&h,0x23);createByte(&h,3);createByte(&h,1);createByte(&h,1);createUInt(&h,5);for(j=0;j<5;j++)createByte(&h,entries[i].properties[j]);}
    else {createByte(&h,1);createByte(&h,0);}
   }
  }
  createByte(&h,12);for(i=0;i<count;i++)if(entries[i].size){if(encrypt){createUInt(&h,entries[i].lzmaUnpack);createUInt(&h,entries[i].size);}else createUInt(&h,entries[i].size);}
  createByte(&h,10);createByte(&h,1);for(i=0;i<count;i++)if(entries[i].size)createLE(&h,entries[i].crc,4);
  createByte(&h,0);createByte(&h,0);
 }
 createByte(&h,5);createUInt(&h,count);
 if(empty){
  createByte(&h,14);createUInt(&h,(count+7)/8);
  for(i=0;i<count;i+=8){unsigned bits=0;for(j=0;j<8&&i+j<count;j++)if(!entries[i+j].size)bits|=0x80>>j;createByte(&h,bits);}
  createByte(&h,15);createUInt(&h,(empty+7)/8);
  {unsigned bit=0,bits=0;for(i=0;i<count;i++)if(!entries[i].size){if(!entries[i].directory)bits|=0x80>>bit;if(++bit==8){createByte(&h,bits);bit=bits=0;}}if(bit)createByte(&h,bits);}
 }
 {UInt64 names=1;for(i=0;i<count;i++)names+=2*((unsigned)strlen(entries[i].name)+1);
 createByte(&h,17);createUInt(&h,names);createByte(&h,0);
 for(i=0;i<count;i++)for(j=0;;j++){createLE(&h,(unsigned char)entries[i].name[j],2);if(!entries[i].name[j])break;}}
 createByte(&h,20);createUInt(&h,2+(UInt64)8*count);createByte(&h,1);createByte(&h,0);
 for(i=0;i<count;i++)createLE(&h,entries[i].mtime,8);
 createByte(&h,21);createUInt(&h,2+(UInt64)4*count);createByte(&h,1);createByte(&h,0);
 for(i=0;i<count;i++)createLE(&h,entries[i].attributes,4);
 createByte(&h,0);createByte(&h,0);
 if(h.failed){result=SZ_ERROR_MEM;goto done;}
 if(!createWriteBytes(output,h.data,h.used)){result=SZ_ERROR_WRITE;goto done;}
 memcpy(signature,"7z\xBC\xAF\x27\x1C",6);signature[6]=0;signature[7]=4;
 start.data=signature;start.used=12;start.failed=0;
 createLE(&start,packed,8);createLE(&start,h.used,8);createLE(&start,CrcCalc(h.data,h.used),4);
 start.used=8;createLE(&start,CrcCalc(signature+12,20),4);
 if(fseek(output,0,SEEK_SET)||!createWriteBytes(output,signature,32)){result=SZ_ERROR_WRITE;goto done;}
 if(fclose(output)){output=NULL;result=SZ_ERROR_WRITE;goto done;}output=NULL;
 ISzAlloc_Free(allocator,h.data);h.data=NULL;
 if(!buffer){buffer=(Byte *)ISzAlloc_Alloc(allocator,4096);if(!buffer){result=SZ_ERROR_MEM;goto done;}}
 {Input in;CLookToRead2 look;CSzArEx verify;
  in.file=fopen(destination,"rb");if(!in.file){result=SZ_ERROR_READ;goto done;}in.iface.Read=readInput;in.iface.Seek=seekInput;
  LookToRead2_CreateVTable(&look,False);look.realStream=&in.iface;look.buf=buffer;look.bufSize=4096;LookToRead2_INIT(&look);SzArEx_Init(&verify);
  result=openArchive(&verify,&in,&look,allocator);
  if(result==SZ_OK&&verify.NumFiles!=count)result=SZ_ERROR_ARCHIVE;
  for(i=0;result==SZ_OK&&i<count;i++)if(SzArEx_GetFileSize(&verify,i)!=entries[i].size||!!SzArEx_IsDir(&verify,i)!=!!entries[i].directory||!inputTreeWideMatch(verify.FileNames+2*verify.FileNameOffsets[i],entries[i].name))result=SZ_ERROR_ARCHIVE;
  /* Encrypted archives are structurally verified (header, names, sizes) but
     not re-decrypted here: a second 2^19 key derivation would roughly double
     creation time, and the ciphertext is produced deterministically. */
  if(result==SZ_OK&&!encrypt)result=testArchive(&verify,&in,&look,allocator);
  SzArEx_Free(&verify,allocator);if(fclose(in.file)&&result==SZ_OK)result=SZ_ERROR_READ;
  if(result!=SZ_OK)goto done;
 }
 strcpy(createMessage,encrypt?"Archive created, AES-256 encrypted (LZMA, 8 KiB dictionary). Modification times and read-only attributes were stored.":lzma?"Archive created using LZMA (8 KiB dictionary). Modification times and read-only attributes were stored.":"Archive created using Copy (no compression). Modification times and read-only attributes were stored.");
 done:
 if(input)fclose(input);
 if(output&&fclose(output)&&result==SZ_OK)result=SZ_ERROR_WRITE;
 if(result!=SZ_OK&&!strncmp(createMessage,"Archive created",15))createMessage[0]=0;
 if(result!=SZ_OK&&owned&&remove(destination))strcpy(createMessage,"ERROR: Creation failed and the incomplete archive could not be removed.");
 ISzAlloc_Free(allocator,h.data);ISzAlloc_Free(allocator,buffer);ISzAlloc_Free(allocator,entries);inputTreeFree(&tree,allocator);
 return result;
}
static SRes createArchive(const char *destination,unsigned count,const char * const *sources,ISzAllocPtr allocator){
 return createArchiveMethod(destination,count,sources,allocator,0);
}
#endif
