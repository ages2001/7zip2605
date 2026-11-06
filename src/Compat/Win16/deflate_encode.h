/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_DEFLATE_ENCODE_H
#define Z7_WIN16_DEFLATE_ENCODE_H
#include "../../C/7zTypes.h"
/* RFC 1951 raw Deflate with an 8 KiB dictionary and fixed Huffman codes.
   Input and output are streaming; no allocation exceeds one segment. */
SRes Z7Win16_DeflateEncode(ISeqInStreamPtr input,ISeqOutStreamPtr output,
    ISzAllocPtr allocator);
/* Deflate64 output using the same bounded 8 KiB encoder dictionary. */
SRes Z7Win16_Deflate64Encode(ISeqInStreamPtr input,ISeqOutStreamPtr output,
    ISzAllocPtr allocator);
#endif
