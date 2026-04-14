#ifndef FAST_LOCK_H
#define FAST_LOCK_H

#include <stdatomic.h>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <os/lock.h>
#elif defined(__linux__)
#include <limits.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#else
#include <pthread.h>
#endif
#include "thread_safety.h"

typedef struct TSA_CAPABILITY("mutex") fast_lock {
#if defined(_WIN32) || defined(__linux__)
  atomic_int state;
#elif defined(__APPLE__)
  os_unfair_lock inner;
#else
  pthread_mutex_t inner;
#endif
} fast_lock;

#if defined(_WIN32) || defined(__linux__)
#define FAST_LOCK_INIT {0}
#elif defined(__APPLE__)
#define FAST_LOCK_INIT {OS_UNFAIR_LOCK_INIT}
#else
#define FAST_LOCK_INIT {PTHREAD_MUTEX_INITIALIZER}
#endif

void fast_lock_acquire(fast_lock *lk) TSA_ACQUIRE(lk);
void fast_lock_release(fast_lock *lk) TSA_RELEASE(lk);

#endif
