/* Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_PAGED_DICTIONARY_H
#define Z7_WIN16_PAGED_DICTIONARY_H
#include "../../C/7zTypes.h"

/* A logical 32-bit dictionary with two 4 KiB cache pages. The backing file
   is created exclusively, only when a dirty page must leave the cache. */
typedef struct Z7Win16Dictionary Z7Win16Dictionary;
SRes Z7Win16_DictionaryCreate(Z7Win16Dictionary **out, UInt32 size, ISzAllocPtr alloc);
/* One page is available for real-mode scratch streams. Compression history
   retains the default two-page cache. */
SRes Z7Win16_DictionaryCreateWithPages(Z7Win16Dictionary **out, UInt32 size,
    ISzAllocPtr alloc, unsigned pages);
Byte Z7Win16_DictionaryGet(Z7Win16Dictionary *p, UInt32 position);
void Z7Win16_DictionarySet(Z7Win16Dictionary *p, UInt32 position, Byte value);
SRes Z7Win16_DictionaryRead(Z7Win16Dictionary *p, UInt32 position, Byte *out, SizeT size);
SRes Z7Win16_DictionaryWrite(Z7Win16Dictionary *p, UInt32 position, const Byte *in, SizeT size);
SRes Z7Win16_DictionaryStatus(const Z7Win16Dictionary *p);
/* Destroy also reports close/remove failures. It never deletes an unowned file. */
SRes Z7Win16_DictionaryDestroy(Z7Win16Dictionary *p, ISzAllocPtr alloc);
#endif
