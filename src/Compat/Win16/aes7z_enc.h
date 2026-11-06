/* 7zAES-256 encryption (create side) for the native engine. Self-contained and
   included only by the create path, so it never touches the decode unit.
   Uses saltSize=0 (key depends only on the password, as desktop 7-Zip also
   does) with a fresh per-file IV; NumCyclesPower = 19. The IV comes from a
   small time-seeded PRNG: adequate for a legacy DOS archiver, not a CSPRNG.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_AES7Z_ENC_H
#define Z7_WIN16_AES7Z_ENC_H
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../../C/7zTypes.h"
#include "../../C/Aes.h"
#include "../../C/Sha256.h"

#define AES7Z_ENC_NCP 19

static Byte g_aes7zEncPwd[512];
static unsigned g_aes7zEncPwdLen=0;
static int g_aes7zEncHas=0;
static void aes7zEncSetPassword(const char *a){
 unsigned n=0;g_aes7zEncPwdLen=0;g_aes7zEncHas=1;
 if(!a){g_aes7zEncHas=0;return;}
 while(a[n]&&2u*n+1<sizeof(g_aes7zEncPwd)){g_aes7zEncPwd[2*n]=(Byte)a[n];g_aes7zEncPwd[2*n+1]=0;n++;}
 g_aes7zEncPwdLen=2*n;
}
static int aes7zEncHasPassword(void){return g_aes7zEncHas;}

/* Key derivation with saltSize=0: SHA-256 over password||counter, 2^ncp times. */
static void aes7zEncDeriveKey(unsigned ncp,Byte key[32]){
 CSha256 sha;Byte buf[520];unsigned bufSize,i;Byte *ctr;UInt32 rounds;
 bufSize=g_aes7zEncPwdLen+8;
 memcpy(buf,g_aes7zEncPwd,g_aes7zEncPwdLen);ctr=buf+g_aes7zEncPwdLen;memset(ctr,0,8);
 Sha256_Init(&sha);rounds=(ncp>=32)?0:((UInt32)1<<ncp);
 while(rounds){Sha256_Update(&sha,buf,bufSize);for(i=0;i<8;i++)if(++ctr[i]!=0)break;rounds--;}
 Sha256_Final(&sha,key);
}

static unsigned long g_aes7zRng=0;
static void aes7zRngSeed(void){g_aes7zRng=(unsigned long)time((time_t *)0)^0x9E3779B9UL;if(!g_aes7zRng)g_aes7zRng=1;}
static Byte aes7zRngByte(void){g_aes7zRng=g_aes7zRng*1103515245UL+12345UL;return (Byte)(g_aes7zRng>>16);}

/* AES-256-CBC-encrypt an LZMA temp stream to out (padded to 16). Fills iv[16]
   and *cipherLen. Key derived from the stored password at NumCyclesPower ncp. */
static SRes aes7zEncryptStream(FILE *tmp,UInt64 lzmaSize,FILE *out,unsigned ncp,Byte iv[16],UInt64 *cipherLen,ISzAllocPtr alloc){
 Byte key[32];UInt32 ivAes[AES_NUM_IVMRK_WORDS+8];UInt32 *aes;unsigned long off;
 Byte block[512];UInt64 left=lzmaSize,total=0;unsigned i;
 (void)alloc;
 for(i=0;i<16;i++)iv[i]=aes7zRngByte();
 AesGenTables();aes7zEncDeriveKey(ncp,key);
 off=(unsigned long)(16-(((unsigned long)(size_t)(void *)ivAes)&15))&15;aes=(UInt32 *)((Byte *)ivAes+off);
 Aes_SetKey_Enc(aes+4,key,32);AesCbc_Init(aes,iv);
 if(fseek(tmp,0,SEEK_SET)){memset(key,0,sizeof(key));return SZ_ERROR_READ;}
 while(left){
  size_t want=left<512?(size_t)left:512,got,enc;
  got=fread(block,1,want,tmp);if(got!=want){memset(key,0,sizeof(key));return SZ_ERROR_READ;}
  enc=(got+15)&~(size_t)15;if(enc>got)memset(block+got,0,enc-got);
  g_AesCbc_Encode(aes,block,enc/16);
  if(fwrite(block,1,enc,out)!=enc){memset(key,0,sizeof(key));return SZ_ERROR_WRITE;}
  total+=enc;left-=got;
 }
 memset(key,0,sizeof(key));
 *cipherLen=total;return SZ_OK;
}
#endif
