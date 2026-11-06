/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_DEFLATE_STREAM_H
#define Z7_WIN16_DEFLATE_STREAM_H
#include "../../C/7zTypes.h"
/* RFC 1951 raw DEFLATE, exact bounded streams. No zlib wrapper. */
SRes Z7Win16_Inflate(ISeekInStreamPtr input, ISeqOutStreamPtr output,
    UInt64 packedSize, UInt64 unpackedSize, ISzAllocPtr alloc, UInt32 *crc);
/* A paged history retains all 32 KiB with two 4 KiB cache pages and a private
   backing file. Nonpaged history is entirely in RAM. Both check cleanup errors. */
SRes Z7Win16_InflateWithCache(ISeekInStreamPtr input, ISeqOutStreamPtr output,
    UInt64 packedSize, UInt64 unpackedSize, ISzAllocPtr alloc, UInt32 *crc, int paged);
/* Deflate64 extends the history to 64 KiB and length code 285 to 16 bits. */
SRes Z7Win16_Inflate64(ISeekInStreamPtr input, ISeqOutStreamPtr output,
    UInt64 packedSize, UInt64 unpackedSize, ISzAllocPtr alloc, UInt32 *crc);
SRes Z7Win16_Inflate64WithCache(ISeekInStreamPtr input, ISeqOutStreamPtr output,
    UInt64 packedSize, UInt64 unpackedSize, ISzAllocPtr alloc, UInt32 *crc, int paged);
#endif
