// SPDX-License-Identifier: MPL-2.0

#ifndef FAST_LOCK_H
#define FAST_LOCK_H
#include "thread_safety.h"
#ifdef _WIN32
#include <windows.h>
#elifdef __APPLE__
#include <os/lock.h>
#elifndef __linux__
#include <pthread.h>
#endif

typedef struct TSA_CAPABILITY("mutex") fast_lock {
#ifdef _WIN32
  _Alignas(4) LONG state;
#elifdef __APPLE__
  os_unfair_lock inner;
#elifdef __linux__
  _Alignas(4) int state;
#else
  pthread_mutex_t inner;
#endif
} fast_lock;

#if defined(_WIN32) || defined(__linux__)
#define FAST_LOCK_INIT {0}
#elifdef __APPLE__
#define FAST_LOCK_INIT {OS_UNFAIR_LOCK_INIT}
#else
#define FAST_LOCK_INIT {PTHREAD_MUTEX_INITIALIZER}
#endif

void fast_lock_acquire(fast_lock *lk) TSA_ACQUIRE(lk);
void fast_lock_release(fast_lock *lk) TSA_RELEASE(lk);

#endif