/* Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#ifdef __WATCOMC__
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#endif
#include "paged_dictionary.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif
#define DICT_PAGE 4096
#define DICT_EMPTY ((UInt32)0xFFFFFFFFUL)
struct Z7Win16Dictionary {
  UInt32 capacity, validSize, page[2];
  unsigned dirty[2], recent, numPages;
  Byte *bytes[2];
  FILE *file;
  char path[260];
  SRes error;
};

static SRes dictionaryError(Z7Win16Dictionary *p, SRes error)
{
  if (!p->error) p->error = error;
  return p->error;
}

static SRes dictionaryOpen(Z7Win16Dictionary *p)
{
  static UInt32 serial;
  const char *directory = getenv("TEMP");
  size_t length;
  unsigned attempt;
  UInt32 name;
  if (p->file) return SZ_OK;
  if (!directory || !*directory) directory = getenv("TMP");
  if (!directory || !*directory) directory = ".";
  length = strlen(directory);
  if (length + 14 >= sizeof(p->path)) return dictionaryError(p, SZ_ERROR_WRITE);
  memcpy(p->path, directory, length);
  if (length && directory[length-1] != '/' && directory[length-1] != '\\'
      && directory[length-1] != ':') {
#ifdef __WATCOMC__
    p->path[length++] = '\\';
#else
    p->path[length++] = '/';
#endif
  }
  /* Names need uniqueness, not entropy. Exclusive creation establishes ownership. */
  name = (UInt32)time(NULL) + serial++;
  for (attempt = 0; attempt < 256; ++attempt) {
    int descriptor;
    sprintf(p->path + length, "7Z%06lX.DIC", (unsigned long)((name + attempt) & 0xFFFFFFUL));
#ifdef __WATCOMC__
    descriptor = _sopen(p->path, O_CREAT | O_EXCL | O_RDWR | O_BINARY,
        SH_DENYRW, S_IREAD | S_IWRITE);
#else
    descriptor = open(p->path, O_CREAT | O_EXCL | O_RDWR | O_BINARY, 0600);
#endif
    if (descriptor >= 0) {
      p->file = fdopen(descriptor, "w+b");
      if (!p->file) {
        close(descriptor);
        if (remove(p->path) == 0) p->path[0] = 0;
        return dictionaryError(p, SZ_ERROR_WRITE);
      }
      setvbuf(p->file, NULL, _IONBF, 0);
      return SZ_OK;
    }
    if (errno != EEXIST) break;
  }
  /* No file was created: the final attempted name is not ours to remove. */
  p->path[0] = 0;
  return dictionaryError(p, SZ_ERROR_WRITE);
}

static SRes dictionarySeek(Z7Win16Dictionary *p, UInt32 page)
{
  UInt32 position = page << 12;
  /* Watcom's DOS file API uses signed 32-bit offsets on every target. */
  if (position > (UInt32)0x7FFFFFFFUL - (DICT_PAGE - 1))
    return dictionaryError(p, SZ_ERROR_UNSUPPORTED);
  if (fseek(p->file, (long)position, SEEK_SET) != 0)
    return dictionaryError(p, SZ_ERROR_READ);
  return SZ_OK;
}

static Byte *dictionaryPage(Z7Win16Dictionary *p, UInt32 position, int writing)
{
  UInt32 page = position >> 12;
  unsigned slot;
  Byte *bytes;
  if (p->error) return NULL;
  if (position >= p->capacity || position > p->validSize
      || (!writing && position == p->validSize)) {
    dictionaryError(p, SZ_ERROR_DATA);
    return NULL;
  }
  if (p->page[0] == page) slot = 0;
  else if (p->page[1] == page) slot = 1;
  else {
    slot = p->page[0] == DICT_EMPTY || p->numPages == 1 ? 0 : p->page[1] == DICT_EMPTY ? 1 : 1 - p->recent;
    bytes = p->bytes[slot];
    if (p->dirty[slot]) {
      if (dictionaryOpen(p) != SZ_OK || dictionarySeek(p, p->page[slot]) != SZ_OK) return NULL;
      if (fwrite(bytes, 1, DICT_PAGE, p->file) != DICT_PAGE) {
        dictionaryError(p, SZ_ERROR_WRITE);
        return NULL;
      }
    }
    p->page[slot] = DICT_EMPTY;
    p->dirty[slot] = 0;
    if ((page << 12) < p->validSize) {
      if (!p->file) {
        dictionaryError(p, SZ_ERROR_FAIL);
        return NULL;
      }
      if (dictionarySeek(p, page) != SZ_OK) return NULL;
      if (fread(bytes, 1, DICT_PAGE, p->file) != DICT_PAGE) {
        dictionaryError(p, SZ_ERROR_READ);
        return NULL;
      }
    } else memset(bytes, 0, DICT_PAGE);
    p->page[slot] = page;
  }
  p->recent = slot;
  if (writing) p->dirty[slot] = 1;
  return p->bytes[slot];
}

SRes Z7Win16_DictionaryCreate(Z7Win16Dictionary **out, UInt32 size, ISzAllocPtr alloc)
{
  return Z7Win16_DictionaryCreateWithPages(out, size, alloc, 2);
}

SRes Z7Win16_DictionaryCreateWithPages(Z7Win16Dictionary **out, UInt32 size,
    ISzAllocPtr alloc, unsigned pages)
{
  Z7Win16Dictionary *p; unsigned i;
  *out = NULL;
  if (!size || (pages != 1 && pages != 2)) return SZ_ERROR_PARAM;
  p = (Z7Win16Dictionary *)ISzAlloc_Alloc(alloc, sizeof(*p));
  if (!p) return SZ_ERROR_MEM;
  memset(p, 0, sizeof(*p));
  p->numPages = pages;
  /* Separate pages avoid requiring an 8 KiB contiguous block in the
     fragmented real-mode global heap. Cache capacity is unchanged. */
  for (i = 0; i < pages; ++i) {
    p->bytes[i] = (Byte *)ISzAlloc_Alloc(alloc, DICT_PAGE);
    if (!p->bytes[i]) {
      while (i) ISzAlloc_Free(alloc, p->bytes[--i]);
      ISzAlloc_Free(alloc, p); return SZ_ERROR_MEM;
    }
  }
  p->capacity = size;
  p->page[0] = p->page[1] = DICT_EMPTY;
  *out = p;
  return SZ_OK;
}

Byte Z7Win16_DictionaryGet(Z7Win16Dictionary *p, UInt32 position)
{
  Byte *bytes = dictionaryPage(p, position, 0);
  return bytes ? bytes[(unsigned)(position & (DICT_PAGE - 1))] : 0;
}

void Z7Win16_DictionarySet(Z7Win16Dictionary *p, UInt32 position, Byte value)
{
  Byte *bytes = dictionaryPage(p, position, 1);
  if (!bytes) return;
  bytes[(unsigned)(position & (DICT_PAGE - 1))] = value;
  if (position == p->validSize) ++p->validSize;
}

SRes Z7Win16_DictionaryRead(Z7Win16Dictionary *p, UInt32 position, Byte *out, SizeT size)
{
  if (position > p->validSize || size > p->validSize - position)
    return dictionaryError(p, SZ_ERROR_DATA);
  while (size && !p->error) {
    unsigned offset = (unsigned)(position & (DICT_PAGE - 1));
    SizeT amount = DICT_PAGE - offset;
    Byte *bytes = dictionaryPage(p, position, 0);
    if (!bytes) break;
    if (amount > size) amount = size;
    memcpy(out, bytes + offset, amount);
    position += amount; out += amount; size -= amount;
  }
  return p->error;
}

SRes Z7Win16_DictionaryWrite(Z7Win16Dictionary *p, UInt32 position, const Byte *in, SizeT size)
{
  if (position > p->validSize || position > p->capacity || size > p->capacity - position)
    return dictionaryError(p, SZ_ERROR_DATA);
  while (size && !p->error) {
    unsigned offset = (unsigned)(position & (DICT_PAGE - 1));
    SizeT amount = DICT_PAGE - offset;
    Byte *bytes = dictionaryPage(p, position, 1);
    if (!bytes) break;
    if (amount > size) amount = size;
    memcpy(bytes + offset, in, amount);
    position += amount; in += amount; size -= amount;
    if (position > p->validSize) p->validSize = position;
  }
  return p->error;
}

SRes Z7Win16_DictionaryStatus(const Z7Win16Dictionary *p) { return p ? p->error : SZ_OK; }

SRes Z7Win16_DictionaryDestroy(Z7Win16Dictionary *p, ISzAllocPtr alloc)
{
  SRes result;
  if (!p) return SZ_OK;
  if (p->file && fclose(p->file) != 0) dictionaryError(p, SZ_ERROR_WRITE);
  if (p->path[0] && remove(p->path) != 0) dictionaryError(p, SZ_ERROR_WRITE);
  result = p->error;
  ISzAlloc_Free(alloc, p->bytes[0]);
  ISzAlloc_Free(alloc, p->bytes[1]);
  ISzAlloc_Free(alloc, p);
  return result;
}
