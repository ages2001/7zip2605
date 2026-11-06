/* Bounded native filesystem operations shared by GUI and integration tests.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#ifndef Z7_WIN16_FILESYSTEM_OPS_H
#define Z7_WIN16_FILESYSTEM_OPS_H
#include "../../C/7zTypes.h"
#define Z7_FS_COPY 1
#define Z7_FS_MOVE 2
#define Z7_FS_RENAME 3
#define Z7_FS_DELETE 4
#define Z7_FS_MKDIR 5
#define Z7_FS_COPY_REPLACE 6
#define Z7_FS_MOVE_REPLACE 7
typedef int (*Z7FsProgress)(UInt64 bytes);
typedef struct {UInt32 files,folders;UInt64 bytes;char message[320];} Z7FsReport;
/* Destination is the complete path. COPY_REPLACE and MOVE_REPLACE permit replacing an
   existing regular file; it stages the copy and preserves the old file until
   commit. MOVE_REPLACE removes the source only after the complete copy is
   published, old-file cleanup succeeds and source identity is rechecked.
   The caller must obtain any required user confirmation beforehand.
   Source is unused for MKDIR; destination is unused for DELETE. */
SRes Z7Win16_FileOperation(unsigned operation,const char *source,const char *destination,
    ISzAllocPtr allocator,Z7FsProgress progress,Z7FsReport *report);
#endif
