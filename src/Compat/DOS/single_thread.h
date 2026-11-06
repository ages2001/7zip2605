/* Single-thread type adapters. Blocking/concurrent operations are unsupported. */
#ifndef Z7_DOS_SINGLE_THREAD_H
#define Z7_DOS_SINGLE_THREAD_H
#ifndef Z7_ST
#error DOS currently requires the upstream single-thread configuration
#endif
#include <errno.h>
typedef unsigned long pthread_t;
typedef struct { unsigned depth; } pthread_mutex_t;
typedef struct { unsigned initialized; } pthread_cond_t;
static inline int pthread_mutex_init(pthread_mutex_t *p, const void *attr) { (void)attr; p->depth=0; return 0; }
static inline int pthread_mutex_destroy(pthread_mutex_t *p) { return p->depth ? EBUSY : 0; }
static inline int pthread_mutex_lock(pthread_mutex_t *p) { ++p->depth; return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *p) { if(!p->depth)return EPERM; --p->depth; return 0; }
static inline int pthread_cond_init(pthread_cond_t *p, const void *attr) { (void)attr; p->initialized=1; return 0; }
static inline int pthread_cond_destroy(pthread_cond_t *p) { p->initialized=0; return 0; }
static inline int pthread_cond_broadcast(pthread_cond_t *p) { return p->initialized ? 0 : EINVAL; }
static inline int pthread_cond_wait(pthread_cond_t *p, pthread_mutex_t *m) { (void)p; (void)m; return ENOSYS; }
#endif
