/* Streaming checksums shared by native DOS and Windows frontends.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_HASH_FILE_H
#define Z7_WIN16_HASH_FILE_H
#include "../../C/7zTypes.h"
#define Z7_WIN16_HASH_COUNT 11
typedef struct {const char *name;unsigned digestSize;} Z7Win16HashMethod;
extern const Z7Win16HashMethod Z7_WIN16_NEAR Z7Win16_HashMethods[Z7_WIN16_HASH_COUNT];
SRes Z7Win16_HashFile(const char *path,unsigned method,Byte *digest,UInt64 *size,ISzAllocPtr alloc,int (*progress)(UInt64));
/* Digests occupy consecutive 64-byte slots, in selected-method order. */
SRes Z7Win16_HashFileMany(const char *path,unsigned mask,Byte *digests,UInt64 *size,ISzAllocPtr alloc,int (*progress)(UInt64));
#endif
