/* Cache eviction and failure tests; also compiled as an 8086 DOS executable. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "paged_dictionary.h"
static int fault;
static unsigned allocations, failAt, live, removed;
static size_t largest;
static size_t testRead(void *out, size_t size, size_t count, FILE *file)
{ return fault == 1 ? 0 : fread(out, size, count, file); }
static size_t testWrite(const void *in, size_t size, size_t count, FILE *file)
{ return fwrite(in, size, fault == 2 && count ? count - 1 : count, file); }
static int testSeek(FILE *file, long offset, int origin)
{ return fault == 3 ? -1 : fseek(file, offset, origin); }
static FILE *testFdopen(int descriptor, const char *mode)
{ return fault == 4 ? NULL : fdopen(descriptor, mode); }
static int testClose(FILE *file)
{ int result = fclose(file); return fault == 5 ? EOF : result; }
static int testRemove(const char *path)
{ int result = remove(path); if (!result) ++removed; return fault == 6 ? -1 : result; }
#define fread testRead
#define fwrite testWrite
#define fseek testSeek
#define fdopen testFdopen
#define fclose testClose
#define remove testRemove
#include "paged_dictionary.c"
#undef remove
#undef fclose
#undef fdopen
#undef fseek
#undef fwrite
#undef fread
static void *allocate(ISzAllocPtr p, size_t n)
{
  void *v; (void)p;
  if (n > largest) largest = n;
  if (++allocations == failAt) return NULL;
  v = malloc(n); if (v) ++live; return v;
}
static void release(ISzAllocPtr p, void *v) { (void)p; if (v) { --live; free(v); } }
static ISzAlloc allocator = { allocate, release };
#define CHECK(condition) do { if (!(condition)) { printf("FAIL dictionary line %u fault %d\n", (unsigned)__LINE__, fault); return 0; } } while (0)
static Byte pattern(UInt32 position, unsigned phase)
{ return (Byte)((position * 17 + (position >> 8)) ^ (phase * 113)); }

static int cacheTest(unsigned pages)
{
  Z7Win16Dictionary *p = NULL, *q = NULL;
  UInt32 position, size = 196613UL;
  unsigned n, i;
  Byte buffer[777];
  char path[260];
  struct stat info;
  CHECK(Z7Win16_DictionaryCreateWithPages(&p, 0xFFFFFFFFUL, &allocator, pages) == SZ_OK);
  for (position = 0; position < pages * 4000U; ++position) Z7Win16_DictionarySet(p, position, pattern(position, 0));
  CHECK(!p->file && !p->path[0]);
  CHECK(Z7Win16_DictionaryGet(p, 0) == pattern(0, 0));
  CHECK(Z7Win16_DictionaryGet(p, pages * 4000U - 1) == pattern(pages * 4000U - 1, 0));
  CHECK(Z7Win16_DictionaryDestroy(p, &allocator) == SZ_OK && !live);
  printf("PASS huge declared capacity uses %u small pages; no file before eviction\n", pages);

  CHECK(Z7Win16_DictionaryCreateWithPages(&p, size, &allocator, pages) == SZ_OK);
  for (position = 0; position < size; position += n) {
    n = size - position < sizeof(buffer) ? (unsigned)(size - position) : sizeof(buffer);
    for (i = 0; i < n; ++i) buffer[i] = pattern(position + i, 0);
    CHECK(Z7Win16_DictionaryWrite(p, position, buffer, n) == SZ_OK);
  }
  CHECK(p->file && p->path[0]); strcpy(path, p->path);
  /* Reverse traversal crosses all dirty/clean eviction paths and 64 KiB boundaries. */
  for (position = size; position; ) { --position; CHECK(Z7Win16_DictionaryGet(p, position) == pattern(position, 0)); }
  for (position = 0; position < 70000UL; ++position) Z7Win16_DictionarySet(p, position, pattern(position, 1));
  for (position = 0; position < size; position += n) {
    n = size - position < sizeof(buffer) ? (unsigned)(size - position) : sizeof(buffer);
    CHECK(Z7Win16_DictionaryRead(p, position, buffer, n) == SZ_OK);
    for (i = 0; i < n; ++i) CHECK(buffer[i] == pattern(position + i, position + i < 70000UL));
  }
  CHECK(Z7Win16_DictionaryCreateWithPages(&q, size, &allocator, pages) == SZ_OK);
  for (position = 0; position < 10000; ++position) Z7Win16_DictionarySet(q, position, 99);
  CHECK(q->file && strcmp(q->path, p->path));
  CHECK(Z7Win16_DictionaryGet(p, 12345) == pattern(12345, 1));
  CHECK(Z7Win16_DictionaryGet(q, 1234) == 99);
  CHECK(Z7Win16_DictionaryDestroy(q, &allocator) == SZ_OK);
  CHECK(Z7Win16_DictionaryDestroy(p, &allocator) == SZ_OK && !live && stat(path, &info) != 0);
  puts("PASS cross-page bulk I/O, reverse reads, wrapped overwrite and simultaneous stores");
  return 1;
}

static int failureTest(unsigned pages)
{
  Z7Win16Dictionary *p;
  unsigned i, before;
  UInt32 position;
  SRes result;
  for (i = 1; i <= pages + 1; ++i) {
    allocations = 0; failAt = i; p = NULL;
    CHECK(Z7Win16_DictionaryCreateWithPages(&p, 131072UL, &allocator, pages) == SZ_ERROR_MEM && !p && !live);
  }
  failAt = 0;
  for (i = 1; i <= 6; ++i) {
    CHECK(Z7Win16_DictionaryCreateWithPages(&p, 131072UL, &allocator, pages) == SZ_OK);
    before = removed;
    if (i == 2 || i == 3 || i == 4) fault = i;
    for (position = 0; position < 20000; ++position) Z7Win16_DictionarySet(p, position, pattern(position, 0));
    fault = i;
    if (i == 1) (void)Z7Win16_DictionaryGet(p, 0);
    result = Z7Win16_DictionaryDestroy(p, &allocator);
    CHECK(result == (i == 1 || i == 3 ? SZ_ERROR_READ : SZ_ERROR_WRITE));
    CHECK(!live && removed == before + 1);
    fault = 0;
  }
  CHECK(Z7Win16_DictionaryCreateWithPages(&p, 131072UL, &allocator, pages) == SZ_OK);
  (void)Z7Win16_DictionaryGet(p, 0);
  CHECK(Z7Win16_DictionaryStatus(p) == SZ_ERROR_DATA);
  CHECK(Z7Win16_DictionaryDestroy(p, &allocator) == SZ_ERROR_DATA && !live);
  CHECK(Z7Win16_DictionaryCreateWithPages(&p, 131072UL, &allocator, pages) == SZ_OK);
  Z7Win16_DictionarySet(p, 1, 0);
  CHECK(Z7Win16_DictionaryDestroy(p, &allocator) == SZ_ERROR_DATA && !live);
  CHECK(Z7Win16_DictionaryCreateWithPages(&p, 1, &allocator, pages) == SZ_OK);
  Z7Win16_DictionarySet(p, 0, 77); Z7Win16_DictionarySet(p, 1, 0);
  CHECK(Z7Win16_DictionaryDestroy(p, &allocator) == SZ_ERROR_DATA && !live);
  CHECK(largest == 4096U);
  puts("PASS allocation, short-read/write, seek, fdopen, close, remove and bounds failures; no allocation leaks");
  return 1;
}
int main(void)
{
  unsigned pages;
  for (pages = 1; pages <= 2; ++pages) {
    largest = 0;
    if (!cacheTest(pages) || !failureTest(pages)) return 2;
  }
  puts("PASS all paged dictionary tests");
  return 0;
}
