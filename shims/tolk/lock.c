// SPDX-License-Identifier: MPL-2.0
#include "lock.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(atomic_int) == sizeof(int), "atomic_int size mismatch");
_Static_assert(sizeof(int) == 4, "futex requires 32-bit int");

void fast_lock_acquire(fast_lock *lk) TSA_NO_THREAD_SAFETY_ANALYSIS {
#ifdef _WIN32
  int expected = 0;
  if (atomic_compare_exchange_strong_explicit(
          &lk->state, &expected, 1, memory_order_acquire, memory_order_relaxed))
    return;
  while (true) {
    if (expected == 2 ||
        (expected == 1 && atomic_compare_exchange_strong_explicit(
                              &lk->state, &expected, 2, memory_order_relaxed,
                              memory_order_relaxed))) {
      int cmp = 2;
      WaitOnAddress((volatile void *)&lk->state, &cmp, sizeof(int), INFINITE);
    }
    expected = 0;
    if (atomic_compare_exchange_strong_explicit(&lk->state, &expected, 2,
                                                memory_order_acquire,
                                                memory_order_relaxed))
      return;
  }
#elif defined(__APPLE__)
  os_unfair_lock_lock(&lk->inner);
#elif defined(__linux__)
  int expected = 0;
  if (atomic_compare_exchange_strong_explicit(
          &lk->state, &expected, 1, memory_order_acquire, memory_order_relaxed))
    return;
  while (true) {
    if (expected == 2 ||
        (expected == 1 && atomic_compare_exchange_strong_explicit(
                              &lk->state, &expected, 2, memory_order_relaxed,
                              memory_order_relaxed))) {
      syscall(SYS_futex, &lk->state, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 2, NULL,
              NULL, 0);
    }
    expected = 0;
    if (atomic_compare_exchange_strong_explicit(&lk->state, &expected, 2,
                                                memory_order_acquire,
                                                memory_order_relaxed))
      return;
  }
#else
  pthread_mutex_lock(&lk->inner);
#endif
}

void fast_lock_release(fast_lock *lk) TSA_NO_THREAD_SAFETY_ANALYSIS {
#ifdef _WIN32
  if (atomic_fetch_sub_explicit(&lk->state, 1, memory_order_release) != 1) {
    atomic_store_explicit(&lk->state, 0, memory_order_release);
    WakeByAddressSingle((void *)&lk->state);
  }
#elif defined(__APPLE__)
  os_unfair_lock_unlock(&lk->inner);
#elif defined(__linux__)
  if (atomic_fetch_sub_explicit(&lk->state, 1, memory_order_release) != 1) {
    atomic_store_explicit(&lk->state, 0, memory_order_release);
    syscall(SYS_futex, &lk->state, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, NULL,
            NULL, 0);
  }
#else
  pthread_mutex_unlock(&lk->inner);
#endif
}
