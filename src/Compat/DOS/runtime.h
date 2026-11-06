/* Missing wide-character operations for the DJGPP C runtime. */
#ifndef Z7_DOS_RUNTIME_H
#define Z7_DOS_RUNTIME_H
#include <stddef.h>
#include <wchar.h>
#include <time.h>
struct timespec { time_t tv_sec; long tv_nsec; };
#define UTIME_NOW 1073741823L
#define UTIME_OMIT 1073741822L
#define AT_FDCWD (-100)
#define AT_SYMLINK_NOFOLLOW 0x100
#ifdef __cplusplus
extern "C" {
#endif
wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t count);
wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t count);
int z7_dos_validate_path(const char *path);
int futimens(int fd, const struct timespec times[2]);
int utimensat(int fd, const char *path, const struct timespec times[2], int flags);
int wcscmp(const wchar_t *a, const wchar_t *b);
wchar_t *wcsstr(const wchar_t *text, const wchar_t *find);
wint_t towupper(wint_t value);
#ifdef __cplusplus
}
#endif
#endif
