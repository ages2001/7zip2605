/* Streaming adapter for Copy/LZMA/LZMA2/Deflate/Deflate64, followed by up to three
   shared branch/delta filters, plus disk-backed four-input BCJ2 decoding. */
#ifndef Z7_WIN16_STREAM_FOLDER_H
#define Z7_WIN16_STREAM_FOLDER_H
#include "../../C/7z.h"
SRes Z7Win16_DecodeFolder(const CSzArEx *archive, UInt32 folderIndex,
    ISeekInStreamPtr input, ISeqOutStreamPtr output, ISzAllocPtr allocator);
typedef int (*Z7Win16DecodeProgress)(UInt64 bytes);
/* Progress also runs during BCJ2 branch preparation, before final output. */
SRes Z7Win16_DecodeFolderProgress(const CSzArEx *archive, UInt32 folderIndex,
    ISeekInStreamPtr input, ISeqOutStreamPtr output, ISzAllocPtr allocator,
    Z7Win16DecodeProgress progress);
#endif
