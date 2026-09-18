// SPDX-License-Identifier: MPL-2.0

#ifdef __linux__
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif
#include "lock.h"
#include <stdbool.h>
#ifdef __linux__
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif
typedef enum {
  FAST_LOCK_UNLOCKED = 0,
  FAST_LOCK_LOCKED = 1,
  FAST_LOCK_CONTENDED = 2,
} fast_lock_state;
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#ifdef _WIN32
_Static_assert(sizeof(LONG) == 4, "fast_lock requires a 32-bit Windows LONG");
#elifdef __linux__
_Static_assert(sizeof(int) == 4,
               "fast_lock requires a 32-bit Linux futex word");
#endif

void fast_lock_acquire(fast_lock *lk) TSA_NO_THREAD_SAFETY_ANALYSIS {
#ifdef _WIN32
  fast_lock_state state = InterlockedCompareExchange(
      &lk->state, FAST_LOCK_LOCKED, FAST_LOCK_UNLOCKED);
  if (state == FAST_LOCK_UNLOCKED)
    return;
  while (true) {
    if (state == FAST_LOCK_CONTENDED ||
        (state == FAST_LOCK_LOCKED &&
         InterlockedCompareExchange(&lk->state, FAST_LOCK_CONTENDED,
                                    FAST_LOCK_LOCKED) != FAST_LOCK_UNLOCKED)) {
      fast_lock_state compare = FAST_LOCK_CONTENDED;
      (void)WaitOnAddress(&lk->state, &compare, sizeof(compare), INFINITE);
    }
    state = InterlockedCompareExchange(&lk->state, FAST_LOCK_CONTENDED,
                                       FAST_LOCK_UNLOCKED);
    if (state == FAST_LOCK_UNLOCKED)
      return;
  }
#elifdef __APPLE__
  os_unfair_lock_lock(&lk->inner);
#elifdef __linux__
  int expected = FAST_LOCK_UNLOCKED;
  if (__atomic_compare_exchange_n(&lk->state, &expected, FAST_LOCK_LOCKED,
                                  false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
    return;
  while (true) {
    if (expected == FAST_LOCK_CONTENDED ||
        (expected == FAST_LOCK_LOCKED &&
         __atomic_compare_exchange_n(&lk->state, &expected, FAST_LOCK_CONTENDED,
                                     false, __ATOMIC_RELAXED,
                                     __ATOMIC_RELAXED))) {
      (void)syscall(SYS_futex, &lk->state, FUTEX_WAIT | FUTEX_PRIVATE_FLAG,
                    FAST_LOCK_CONTENDED, NULL, NULL, 0);
    }
    expected = FAST_LOCK_UNLOCKED;
    if (__atomic_compare_exchange_n(&lk->state, &expected, FAST_LOCK_CONTENDED,
                                    false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
      return;
  }
#else
  (void)pthread_mutex_lock(&lk->inner);
#endif
}

void fast_lock_release(fast_lock *lk) TSA_NO_THREAD_SAFETY_ANALYSIS {
#ifdef _WIN32
  const fast_lock_state previous =
      InterlockedExchange(&lk->state, FAST_LOCK_UNLOCKED);
  if (previous == FAST_LOCK_CONTENDED)
    WakeByAddressSingle(&lk->state);
#elifdef __APPLE__
  os_unfair_lock_unlock(&lk->inner);
#elifdef __linux__
  const fast_lock_state previous =
      __atomic_exchange_n(&lk->state, FAST_LOCK_UNLOCKED, __ATOMIC_RELEASE);
  if (previous == FAST_LOCK_CONTENDED) {
    (void)syscall(SYS_futex, &lk->state, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1,
                  NULL, NULL, 0);
  }
#else
  (void)pthread_mutex_unlock(&lk->inner);
#endif
}
