/* 7zAES (method 06F10701) decryption for the native engine: SHA-256 key
   derivation (2^NumCyclesPower rounds over salt||password||counter) and an
   AES-256-CBC decrypting input-stream wrapper that sits between the packed 7z
   stream and the real codec (LZMA/LZMA2/...). Password is held UTF-16LE.
   Decrypt only; uses the shared C AES and SHA-256. Read side only.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_AES7Z_H
#define Z7_WIN16_AES7Z_H
#include <string.h>
#include "../../C/7zTypes.h"
#include "../../C/Aes.h"
#include "../../C/Sha256.h"

#define METHOD_AES256SHA256 0x06F10701UL
#define SZ_ERROR_WIN16_PASSWORD 100

/* password as UTF-16LE bytes (what 7z hashes); set by the front end.
   Used only within the stream_folder translation unit that includes this. */
static Byte g_aes7zPwd[512];
static unsigned g_aes7zPwdLen=0;      /* bytes of UTF-16LE */
static int g_aes7zHasPwd=0;
/* Non-static so the CLI/GUI front end can set the password from another unit. */
void aes7zSetPassword(const char *ascii){
 unsigned n=0;g_aes7zPwdLen=0;g_aes7zHasPwd=1;
 if(!ascii){g_aes7zHasPwd=0;return;}
 while(ascii[n]&&2u*n+1<sizeof(g_aes7zPwd)){g_aes7zPwd[2*n]=(Byte)ascii[n];g_aes7zPwd[2*n+1]=0;n++;}
 g_aes7zPwdLen=2*n;
}

/* Parse 7zAES coder properties -> numCyclesPower, salt, iv[16]. */
static SRes aes7zParseProps(const Byte *p,size_t size,unsigned *numCyclesPower,Byte *salt,unsigned *saltSize,Byte iv[16]){
 unsigned b0,b1,ss,is,i;
 memset(iv,0,16);*saltSize=0;*numCyclesPower=0;
 if(size==0)return SZ_ERROR_UNSUPPORTED;
 b0=p[0];*numCyclesPower=b0&0x3F;
 if((b0&0xC0)==0)return size==1?SZ_OK:SZ_ERROR_DATA;
 if(size<=1)return SZ_ERROR_DATA;
 b1=p[1];ss=((b0>>7)&1)+(b1>>4);is=((b0>>6)&1)+(b1&0x0F);
 if(size!=(size_t)2+ss+is)return SZ_ERROR_DATA;
 if(ss>16||is>16)return SZ_ERROR_UNSUPPORTED;
 p+=2;for(i=0;i<ss;i++)salt[i]=*p++;*saltSize=ss;
 for(i=0;i<is;i++)iv[i]=*p++;
 return SZ_OK;
}

/* Derive the AES-256 key (simple, non-unrolled 7z algorithm). */
static void aes7zDeriveKey(unsigned numCyclesPower,const Byte *salt,unsigned saltSize,Byte key[32]){
 CSha256 sha;Byte buf[8+16+512];unsigned bufSize,i;Byte *ctr;UInt32 roundsHi,roundsLo;
 if(numCyclesPower==0x3F){ /* identity: salt||password, zero padded */
  unsigned pos=0;for(pos=0;pos<saltSize&&pos<32;pos++)key[pos]=salt[pos];
  for(i=0;i<g_aes7zPwdLen&&pos<32;i++)key[pos++]=g_aes7zPwd[i];
  for(;pos<32;pos++)key[pos]=0;return;
 }
 bufSize=saltSize+g_aes7zPwdLen+8;
 memcpy(buf,salt,saltSize);memcpy(buf+saltSize,g_aes7zPwd,g_aes7zPwdLen);
 ctr=buf+saltSize+g_aes7zPwdLen;memset(ctr,0,8);
 Sha256_Init(&sha);
 /* numRounds = 1<<numCyclesPower, iterated as a 64-bit-safe loop (power<=24) */
 roundsHi=0;roundsLo=(numCyclesPower>=32)?0:((UInt32)1<<numCyclesPower);
 (void)roundsHi;
 while(roundsLo){
  Sha256_Update(&sha,buf,bufSize);
  for(i=0;i<8;i++)if(++ctr[i]!=0)break;
  roundsLo--;
 }
 Sha256_Final(&sha,key);
}

/* AES-CBC decrypting ISeekInStream wrapper over an already-positioned input. */
typedef struct {
 ISeekInStream vt;
 ISeekInStreamPtr base;
 UInt32 ivAes[AES_NUM_IVMRK_WORDS+8];   /* +slack; 16-byte align applied below */
 UInt32 *aes;                            /* aligned pointer into ivAes */
 Byte buf[512];                          /* plaintext serving buffer (mult of 16) */
 unsigned bufPos,bufLen;
 UInt64 cipherLeft;                      /* ciphertext bytes not yet pulled */
} Aes7zStream;

static SRes aes7zRefill(Aes7zStream *s){
 size_t want,got;
 s->bufPos=0;s->bufLen=0;
 if(s->cipherLeft==0)return SZ_OK;
 want=s->cipherLeft<sizeof(s->buf)?(size_t)s->cipherLeft:sizeof(s->buf);
 want&=~(size_t)15;if(want==0)return SZ_ERROR_DATA;
 got=want;
 {SRes r=ISeekInStream_Read(s->base,s->buf,&got);if(r!=SZ_OK)return r;}
 if(got==0)return SZ_ERROR_INPUT_EOF;
 if(got&15)return SZ_ERROR_DATA;           /* 7z ciphertext is block-aligned */
 g_AesCbc_Decode(s->aes,s->buf,got/16);
 s->bufLen=(unsigned)got;s->cipherLeft-=got;return SZ_OK;
}
static SRes aes7zRead(ISeekInStreamPtr pp,void *data,size_t *size){
 Aes7zStream *s=(Aes7zStream *)pp;Byte *out=(Byte *)data;size_t need=*size,done=0;
 while(need){
  size_t avail;
  if(s->bufPos>=s->bufLen){if(s->cipherLeft==0)break;{SRes r=aes7zRefill(s);if(r!=SZ_OK){*size=done;return r;}}if(s->bufLen==0)break;}
  avail=s->bufLen-s->bufPos;if(avail>need)avail=need;
  memcpy(out+done,s->buf+s->bufPos,avail);s->bufPos+=(unsigned)avail;done+=avail;need-=avail;
 }
 *size=done;return SZ_OK;
}
static SRes aes7zSeek(ISeekInStreamPtr pp,Int64 *pos,ESzSeek origin){(void)pp;(void)pos;(void)origin;return SZ_ERROR_UNSUPPORTED;}

/* Initialise the wrapper: derive key, set AES CBC, remember ciphertext length.
   Returns SZ_OK, or an error if no password / bad props / unaligned length. */
static SRes aes7zInit(Aes7zStream *s,ISeekInStreamPtr base,const Byte *props,size_t propsSize,UInt64 cipherLen){
 unsigned ncp,ss;Byte salt[16],iv[16],key[32];SRes r;unsigned long off;
 if(!g_aes7zHasPwd)return SZ_ERROR_WIN16_PASSWORD;   /* front end maps this to "password required" */
 r=aes7zParseProps(props,propsSize,&ncp,salt,&ss,iv);if(r!=SZ_OK)return r;
 if(cipherLen&15)return SZ_ERROR_DATA;
 AesGenTables();
 aes7zDeriveKey(ncp,salt,ss,key);
 /* 16-byte align the AES context within ivAes */
 off=(unsigned long)(16-(((unsigned long)(size_t)(void *)s->ivAes)&15))&15;
 s->aes=(UInt32 *)((Byte *)s->ivAes+off);
 Aes_SetKey_Dec(s->aes+4,key,32);
 AesCbc_Init(s->aes,iv);
 s->vt.Read=aes7zRead;s->vt.Seek=aes7zSeek;s->base=base;
 s->bufPos=s->bufLen=0;s->cipherLeft=cipherLen;
 memset(key,0,sizeof(key));
 return SZ_OK;
}
#endif
