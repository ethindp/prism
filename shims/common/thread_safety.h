// SPDX-License-Identifier: MPL-2.0
#ifndef THREAD_SAFETY_H
#define THREAD_SAFETY_H
#if defined(__clang__) && !defined(SWIG)
#define TSA_ATTR(x) __attribute__((x))
#define TSA_CAPABILITY(x) TSA_ATTR(capability(x))
#define TSA_REENTRANT_CAPABILITY TSA_ATTR(reentrant_capability)
#define TSA_SCOPED_CAPABILITY TSA_ATTR(scoped_lockable)
#define TSA_GUARDED_BY(...) TSA_ATTR(guarded_by(__VA_ARGS__))
#define TSA_PT_GUARDED_BY(...) TSA_ATTR(pt_guarded_by(__VA_ARGS__))
#define TSA_ACQUIRED_BEFORE(...) TSA_ATTR(acquired_before(__VA_ARGS__))
#define TSA_ACQUIRED_AFTER(...) TSA_ATTR(acquired_after(__VA_ARGS__))
#define TSA_REQUIRES(...) TSA_ATTR(requires_capability(__VA_ARGS__))
#define TSA_REQUIRES_SHARED(...)                                               \
  TSA_ATTR(requires_shared_capability(__VA_ARGS__))
#define TSA_EXCLUDES(...) TSA_ATTR(locks_excluded(__VA_ARGS__))
#define TSA_ACQUIRE(...) TSA_ATTR(acquire_capability(__VA_ARGS__))
#define TSA_ACQUIRE_SHARED(...) TSA_ATTR(acquire_shared_capability(__VA_ARGS__))
#define TSA_RELEASE(...) TSA_ATTR(release_capability(__VA_ARGS__))
#define TSA_RELEASE_SHARED(...) TSA_ATTR(release_shared_capability(__VA_ARGS__))
#define TSA_RELEASE_GENERIC(...)                                               \
  TSA_ATTR(release_generic_capability(__VA_ARGS__))
#define TSA_TRY_ACQUIRE(...) TSA_ATTR(try_acquire_capability(__VA_ARGS__))
#define TSA_TRY_ACQUIRE_SHARED(...)                                            \
  TSA_ATTR(try_acquire_shared_capability(__VA_ARGS__))
#define TSA_ASSERT_CAPABILITY(x) TSA_ATTR(assert_capability(x))
#define TSA_ASSERT_SHARED_CAPABILITY(x) TSA_ATTR(assert_shared_capability(x))
#define TSA_RETURN_CAPABILITY(x) TSA_ATTR(lock_returned(x))
#define TSA_NO_THREAD_SAFETY_ANALYSIS TSA_ATTR(no_thread_safety_analysis)
#else
#define TSA_CAPABILITY(x)
#define TSA_REENTRANT_CAPABILITY
#define TSA_SCOPED_CAPABILITY
#define TSA_GUARDED_BY(...)
#define TSA_PT_GUARDED_BY(...)
#define TSA_ACQUIRED_BEFORE(...)
#define TSA_ACQUIRED_AFTER(...)
#define TSA_REQUIRES(...)
#define TSA_REQUIRES_SHARED(...)
#define TSA_EXCLUDES(...)
#define TSA_ACQUIRE(...)
#define TSA_ACQUIRE_SHARED(...)
#define TSA_RELEASE(...)
#define TSA_RELEASE_SHARED(...)
#define TSA_RELEASE_GENERIC(...)
#define TSA_TRY_ACQUIRE(...)
#define TSA_TRY_ACQUIRE_SHARED(...)
#define TSA_ASSERT_CAPABILITY(x)
#define TSA_ASSERT_SHARED_CAPABILITY(x)
#define TSA_RETURN_CAPABILITY(x)
#define TSA_NO_THREAD_SAFETY_ANALYSIS
#endif
#endif // THREAD_SAFETY_H