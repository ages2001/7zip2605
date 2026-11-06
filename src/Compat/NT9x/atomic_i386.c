/* Software atomics for an i386 without CMPXCHG or XADD.
   Copyright (c) 2026. SPDX-License-Identifier: LGPL-2.1-or-later
   A process-local lock serializes all operations provided by this module.
   XCHG with a memory operand is implicitly locked on the 80386. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
static volatile uint32_t atomic_lock;
static void acquire(void) {
  for (;;) {
    uint32_t busy = 1;
    __asm__ __volatile__("xchgl %0, %1" : "+r"(busy), "+m"(atomic_lock) : : "memory");
    if (!busy) return;
    Sleep(0);
  }
}
static void release(void) {
  __asm__ __volatile__("" : : : "memory");
  atomic_lock = 0;
}
#define OP(name, expression, result) \
uint32_t name(volatile void *address, uint32_t value) { \
  volatile uint32_t *p = address; uint32_t old, updated; \
  acquire(); old = *p; updated = (expression); *p = updated; release(); return (result); \
}
OP(__sync_add_and_fetch_4, old + value, updated)
OP(__sync_sub_and_fetch_4, old - value, updated)
OP(__sync_fetch_and_add_4, old + value, old)
OP(__sync_fetch_and_sub_4, old - value, old)
OP(__sync_fetch_and_or_4, old | value, old)
OP(__sync_fetch_and_and_4, old & value, old)
OP(__sync_fetch_and_xor_4, old ^ value, old)
uint32_t __sync_val_compare_and_swap_4(volatile void *address, uint32_t expected, uint32_t desired) {
  volatile uint32_t *p = address; uint32_t old;
  acquire(); old = *p; if (old == expected) *p = desired; release(); return old;
}
_Bool __sync_bool_compare_and_swap_4(volatile void *address, uint32_t expected, uint32_t desired) {
  return __sync_val_compare_and_swap_4(address, expected, desired) == expected;
}
uint32_t __atomic_fetch_add_4(volatile void *address, uint32_t value, int order) {
  (void)order; /* The lock provides the strongest ordering for every caller. */
  return __sync_fetch_and_add_4(address, value);
}
