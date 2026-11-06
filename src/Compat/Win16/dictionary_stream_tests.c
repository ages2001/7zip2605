/* Verify paged I/O errors reach the archive stream caller, including teardown. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stream_folder.h"
#include "../../C/7zCrc.h"
static int fault;
static unsigned live;
static size_t dictionaryRead(void *out, size_t size, size_t count, FILE *file)
{ return fault == 1 ? 0 : fread(out, size, count, file); }
static size_t dictionaryWrite(const void *in, size_t size, size_t count, FILE *file)
{ return fwrite(in, size, fault == 2 && count ? count - 1 : count, file); }
static int dictionarySeekFile(FILE *file, long offset, int origin)
{ return fault == 3 ? -1 : fseek(file, offset, origin); }
static FILE *dictionaryFdopen(int descriptor, const char *mode)
{ return fault == 4 ? NULL : fdopen(descriptor, mode); }
static int dictionaryCloseFile(FILE *file)
{ int result = fclose(file); return fault == 5 ? EOF : result; }
static int dictionaryRemoveFile(const char *path)
{ int result = remove(path); return fault == 6 ? -1 : result; }
#define fread dictionaryRead
#define fwrite dictionaryWrite
#define fseek dictionarySeekFile
#define fdopen dictionaryFdopen
#define fclose dictionaryCloseFile
#define remove dictionaryRemoveFile
#include "paged_dictionary.c"
#undef remove
#undef fclose
#undef fdopen
#undef fseek
#undef fwrite
#undef fread
typedef struct { ISeekInStream iface; FILE *file; } Input;
typedef struct { ISeqOutStream iface; UInt32 bytes, crc; } Output;
static SRes inputRead(ISeekInStreamPtr p, void *buffer, size_t *size)
{ Input *s = (Input *)p; *size = fread(buffer, 1, *size, s->file); return ferror(s->file) ? SZ_ERROR_READ : SZ_OK; }
static SRes inputSeek(ISeekInStreamPtr p, Int64 *position, ESzSeek origin)
{
 Input *s = (Input *)p;
 if ((Int64)(long)*position != *position || fseek(s->file, (long)*position, origin)) return SZ_ERROR_READ;
 *position = ftell(s->file); return *position < 0 ? SZ_ERROR_READ : SZ_OK;
}
static size_t outputWrite(ISeqOutStreamPtr p, const void *buffer, size_t size)
{ Output *s = (Output *)p; s->bytes += size; s->crc = CrcUpdate(s->crc, buffer, size); return size; }
static void *allocate(ISzAllocPtr p, size_t size)
{ void *v; (void)p; v = malloc(size); if (v) ++live; return v; }
static void release(ISzAllocPtr p, void *v) { (void)p; if (v) { --live; free(v); } }
int main(int argc, char **argv)
{
 ISzAlloc alloc = { allocate, release };
 int index, which;
 if (argc < 2) return 7;
 CrcGenerateTable();
 for (index = 1; index < argc; ++index) for (which = 0; which <= 6; ++which) {
  Input input; Output output; CLookToRead2 look; CSzArEx archive;
  Byte buffer[4096]; SRes result, expected; UInt64 expectedBytes; UInt32 expectedCrc; int bcj2;
  input.iface.Read = inputRead; input.iface.Seek = inputSeek;
  input.file = fopen(argv[index], "rb"); if (!input.file) return 2;
  LookToRead2_CreateVTable(&look, False); look.realStream = &input.iface;
  look.buf = buffer; look.bufSize = sizeof(buffer); LookToRead2_INIT(&look);
  SzArEx_Init(&archive); fault = 0;
  result = SzArEx_Open(&archive, &look.vt, &alloc, &alloc);
  if (result != SZ_OK || archive.db.NumFolders != 1) return 2;
  expectedBytes = SzAr_GetFolderUnpackSize(&archive.db, 0);
  if (archive.NumFiles != 1 || !SzBitWithVals_Check(&archive.CRCs, 0)) return 2;
  expectedCrc = archive.CRCs.Vals[0]; bcj2 = archive.db.NumPackStreams == 4;
  output.iface.Write = outputWrite; output.bytes = 0; output.crc = CRC_INIT_VAL;
  fault = which;
  result = Z7Win16_DecodeFolder(&archive, 0, &input.iface, &output.iface, &alloc);
  fault = 0; SzArEx_Free(&archive, &alloc); fclose(input.file);
  expected = !which ? SZ_OK : which == 1 || which == 3 ? SZ_ERROR_READ : SZ_ERROR_WRITE;
  if (result != expected || live || (!which && (output.bytes != expectedBytes || CRC_GET_DIGEST(output.crc) != expectedCrc))
      || (!bcj2 && which >= 5 && output.bytes != expectedBytes)) {
   printf("FAIL %s fault=%d result=%d bytes=%lu blocks=%u\n", argv[index], which, result, (unsigned long)output.bytes, live); return 2;
  }
  printf("PASS %s fault=%d result=%d bytes=%lu blocks=0\n", argv[index], which, result, (unsigned long)output.bytes);
 }
 return 0;
}
