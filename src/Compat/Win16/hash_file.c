/* Shared native hash backend. Uses upstream hash implementations.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#ifdef __WATCOMC__
#include <share.h>
#endif
#include "hash_file.h"
#include "../../C/Md5.h"
#include "../../C/Sha1.h"
#include "../../C/Sha256.h"
#include "../../C/Sha512.h"
#include "../../C/Sha3.h"
#include "../../C/Blake2.h"
#include "../../C/Xxh64.h"

#if defined(__WATCOMC__) && defined(__WINDOWS__)
#include <windows.h>
static unsigned hashCs(void);
#pragma aux hashCs = "mov ax,cs" value [ax] modify exact [ax];
#endif

#include "../../C/7zCrc.h"
#include "../../C/XzCrc64.h"
#ifdef Z7_HASH_TRACE
static unsigned hashDs(void);
#pragma aux hashDs = "mov ax,ds" value [ax] modify exact [ax];
static unsigned hashSs(void);
#pragma aux hashSs = "mov ax,ss" value [ax] modify exact [ax];
static void hashTrace(const char *phase,unsigned method){FILE *f=fopen("C:\\H49TRACE.TXT","a");if(f){fprintf(f,"%s %u DS=%04X SS=%04X CRC=%p\n",phase,method,hashDs(),hashSs(),g_CrcTable);fclose(f);}}
#else
#define hashTrace(phase,method) ((void)0)
#endif
const Z7Win16HashMethod Z7_WIN16_NEAR Z7Win16_HashMethods[Z7_WIN16_HASH_COUNT]={
 {"CRC32",4},{"CRC64",8},{"SHA1",20},{"SHA256",32},{"SHA384",48},{"SHA512",64},
 {"SHA3-256",32},{"MD5",16},{"BLAKE2sp",32},{"XXH64",8},{"CKSUM",4}
};
/* Each method keeps only its own context, instead of reserving the largest
   (BLAKE2sp) union for every selected method. All contexts share one compact
   allocation; the header records live on the small caller stack. */
typedef struct {unsigned method;UInt64 count;void *context;} HashState;
#if defined(__WATCOMC__) && defined(__I86__)
#define HASH_ALIGNMENT 2
#else
#define HASH_ALIGNMENT 64
#endif
static const size_t hashStateSizes[Z7_WIN16_HASH_COUNT]={
 sizeof(UInt32),sizeof(UInt64),sizeof(CSha1),sizeof(CSha256),sizeof(CSha512),sizeof(CSha512),
 sizeof(CSha3),sizeof(CMd5),sizeof(CBlake2sp),sizeof(CXxh64),sizeof(UInt32)
};
static size_t hashStateStride(unsigned method){return (hashStateSizes[method]+HASH_ALIGNMENT-1)&~(HASH_ALIGNMENT-1);}
/* POSIX cksum uses the MSB-first CRC polynomial, initial zero, and appends
   the byte length least-significant byte first before complementing. */
static UInt32 checksumByte(UInt32 value,Byte data){unsigned i;value^=(UInt32)data<<24;for(i=0;i<8;i++)value=(value<<1)^((value&0x80000000UL)?0x04c11db7UL:0);return value;}
static void hashBigEndian(Byte *out,UInt64 value,unsigned size){while(size){out[--size]=(Byte)value;value>>=8;}}
static SRes hashInit(HashState *s,unsigned method,void *context){
 s->context=context;
 hashTrace("enter init",method);
 if(method>=Z7_WIN16_HASH_COUNT)return SZ_ERROR_UNSUPPORTED;s->method=method;hashTrace("method set",method);s->count=0;hashTrace("count set",method);
 switch(method){
 case 0:CrcGenerateTable();hashTrace("crc table",method);(*(UInt32 *)s->context)=CRC_INIT_VAL;break;
 case 1:Crc64GenerateTable();(*(UInt64 *)s->context)=CRC64_INIT_VAL;break;
 case 2:Sha1_Init(((CSha1 *)s->context));break;
 case 3:Sha256_Init(((CSha256 *)s->context));break;
 case 4:case 5:Sha512_Init(((CSha512 *)s->context),method==4?48:64);break;
 case 6:Sha3_SET_blockSize(((CSha3 *)s->context),SHA3_BLOCK_SIZE_FROM_DIGEST_SIZE(32));Sha3_Init(((CSha3 *)s->context));break;
 case 7:Md5_Init(((CMd5 *)s->context));break;
 case 8:Blake2sp_Init(((CBlake2sp *)s->context));break;
 case 9:Xxh64_Init(((CXxh64 *)s->context));break;
 case 10:(*(UInt32 *)s->context)=0;break;
 }
 return SZ_OK;
}
static void hashUpdate(HashState *s,const void *data,size_t size){
 const Byte *p=(const Byte *)data;s->count+=size;
 switch(s->method){
 case 0:(*(UInt32 *)s->context)=CrcUpdate((*(UInt32 *)s->context),data,size);break;
 case 1:(*(UInt64 *)s->context)=Crc64Update((*(UInt64 *)s->context),data,size);break;
 case 2:Sha1_Update(((CSha1 *)s->context),p,size);break;
 case 3:Sha256_Update(((CSha256 *)s->context),p,size);break;
 case 4:case 5:Sha512_Update(((CSha512 *)s->context),p,size);break;
 case 6:Sha3_Update(((CSha3 *)s->context),p,size);break;
 case 7:Md5_Update(((CMd5 *)s->context),p,size);break;
 case 8:Blake2sp_Update(((CBlake2sp *)s->context),p,size);break;
 case 9:Xxh64_Update(((CXxh64 *)s->context),data,size);break;
 case 10:while(size--)(*(UInt32 *)s->context)=checksumByte((*(UInt32 *)s->context),*p++);break;
 }
}
static void hashFinal(HashState *s,Byte *digest){
 switch(s->method){
 case 0:hashBigEndian(digest,CRC_GET_DIGEST((*(UInt32 *)s->context)),4);break;
 case 1:hashBigEndian(digest,CRC64_GET_DIGEST((*(UInt64 *)s->context)),8);break;
 case 2:Sha1_Final(((CSha1 *)s->context),digest);break;
 case 3:Sha256_Final(((CSha256 *)s->context),digest);break;
 case 4:case 5:Sha512_Final(((CSha512 *)s->context),digest,s->method==4?48:64);break;
 case 6:Sha3_Final(((CSha3 *)s->context),digest,32,0);break;
 case 7:Md5_Final(((CMd5 *)s->context),digest);break;
 case 8:Blake2sp_Final(((CBlake2sp *)s->context),digest);break;
 case 9:hashBigEndian(digest,Xxh64_Digest(((CXxh64 *)s->context)),8);break;
 case 10:{UInt64 size=s->count;UInt32 crc=(*(UInt32 *)s->context);while(size){crc=checksumByte(crc,(Byte)size);size>>=8;}hashBigEndian(digest,crc^0xffffffffUL,4);break;}
 }
}
SRes Z7Win16_HashFileMany(const char *path,unsigned mask,Byte *digest,UInt64 *size,ISzAllocPtr alloc,int (*progress)(UInt64)){
 HashState state[Z7_WIN16_HASH_COUNT];Byte *contexts=NULL,*cursor,*buffer=NULL;FILE *file=NULL;SRes result=SZ_OK;struct stat before,after;size_t n,stateBytes=0;unsigned count=0,i,m;

#if defined(__WATCOMC__) && defined(__WINDOWS__)
 unsigned hashSegment=0;
#endif
 *size=0;if(!mask||(mask>>Z7_WIN16_HASH_COUNT))return SZ_ERROR_UNSUPPORTED;
 for(m=0;m<Z7_WIN16_HASH_COUNT;m++)if(mask&(1u<<m))stateBytes+=hashStateStride(m);
 if(stat(path,&before)||(before.st_mode&S_IFMT)!=S_IFREG||before.st_size<0)return SZ_ERROR_READ;

#if defined(__WATCOMC__) && defined(__WINDOWS__)
 hashSegment=hashCs();if(!LockSegment(hashSegment))return SZ_ERROR_MEM;
#endif
 /* Process pending window work before reserving checksum memory. In real
    mode, a closing second instance can still own its data/stack at this point. */
 if(progress&&!progress(0)){result=SZ_ERROR_PROGRESS;goto done;}
 contexts=(Byte *)ISzAlloc_Alloc(alloc,stateBytes+HASH_ALIGNMENT-1);buffer=(Byte *)ISzAlloc_Alloc(alloc,4096);
 if(!contexts||!buffer){result=SZ_ERROR_MEM;goto done;}
 cursor=contexts+(size_t)((HASH_ALIGNMENT-((uintptr_t)contexts&(HASH_ALIGNMENT-1)))&(HASH_ALIGNMENT-1));
 for(m=0;m<Z7_WIN16_HASH_COUNT;m++)if(mask&(1u<<m)){hashTrace("init",m);result=hashInit(state+count++,m,cursor);if(result!=SZ_OK)goto done;cursor+=hashStateStride(m);}
#ifdef __WATCOMC__
 hashTrace("before open",0);
 file=_fsopen(path,"rb",SH_DENYWR);
#else
 file=fopen(path,"rb");
#endif
 hashTrace("after open",0);if(!file){result=SZ_ERROR_READ;goto done;}
 for(;;){
  hashTrace("before progress",0);
#ifdef Z7_HASH_TRACE
  {FILE *t=fopen("C:\\H49TRACE.TXT","a");if(t){fprintf(t,"progress=%p state=%p buffer=%p\n",progress,state,buffer);fclose(t);}}
#endif
  if(progress&&!progress(state->count)){result=SZ_ERROR_PROGRESS;goto done;}
  hashTrace("after progress",0);
  n=fread(buffer,1,4096,file);if(!n)break;for(i=0;i<count;i++){hashTrace("update",i);hashUpdate(state+i,buffer,n);}
 }
 if(ferror(file)||stat(path,&after)||before.st_size!=after.st_size||before.st_mtime!=after.st_mtime||state->count!=(UInt64)before.st_size){result=SZ_ERROR_READ;goto done;}
 if(fclose(file)){file=NULL;result=SZ_ERROR_READ;goto done;}file=NULL;
 for(i=0;i<count;i++){hashTrace("final",i);hashFinal(state+i,digest+64*i);}*size=state->count;
 done:if(file)fclose(file);ISzAlloc_Free(alloc,buffer);ISzAlloc_Free(alloc,contexts);
#if defined(__WATCOMC__) && defined(__WINDOWS__)
 UnlockSegment(hashSegment);
#endif
 return result;
}
SRes Z7Win16_HashFile(const char *path,unsigned method,Byte *digest,UInt64 *size,ISzAllocPtr alloc,int (*progress)(UInt64)){
 if(method>=Z7_WIN16_HASH_COUNT){*size=0;return SZ_ERROR_UNSUPPORTED;}
 return Z7Win16_HashFileMany(path,1u<<method,digest,size,alloc,progress);
}
