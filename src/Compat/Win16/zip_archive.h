/* SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_ZIP_ARCHIVE_H
#define Z7_WIN16_ZIP_ARCHIVE_H
#include "../../C/7z.h"
/* Imports a bounded classic ZIP directory into the common file metadata and
   single-stream folder representation. The original archive is never edited. */
SRes Z7Win16_OpenZip(CSzArEx *archive,ISeekInStreamPtr input,ISzAllocPtr allocator,unsigned *extraMetadata);
#endif
