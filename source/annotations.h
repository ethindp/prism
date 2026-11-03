// SPDX-License-Identifier: MPL-2.0

#pragma once

// Lifetime contracts the compiler can check at call sites. Clang reads all of
// them; MSVC reads only lifetimebound, and only under /analyze (see
// PRISM_MSVC_ANALYSIS). Everything else expands to nothing.

// The result refers into this parameter, or into *this when written after the
// function's qualifiers.
#if __has_cpp_attribute(clang::lifetimebound)
#define PRISM_LIFETIMEBOUND [[clang::lifetimebound]]
#elif __has_cpp_attribute(msvc::lifetimebound)
#define PRISM_LIFETIMEBOUND [[msvc::lifetimebound]]
#else
#define PRISM_LIFETIMEBOUND
#endif

// This parameter (or *this) is stored into the named parameter, or into this.
#if __has_cpp_attribute(clang::lifetime_capture_by)
#define PRISM_LIFETIME_CAPTURE_BY(...)                                         \
  [[clang::lifetime_capture_by(__VA_ARGS__)]]
#else
#define PRISM_LIFETIME_CAPTURE_BY(...)
#endif

// A type that views, but does not own, objects of type T.
#if __has_cpp_attribute(gsl::Pointer)
#define PRISM_GSL_POINTER(T) [[gsl::Pointer(T)]]
#else
#define PRISM_GSL_POINTER(T)
#endif

// A member that makes a moved-from object usable again.
#if __has_cpp_attribute(clang::reinitializes)
#define PRISM_REINITIALIZES [[clang::reinitializes]]
#else
#define PRISM_REINITIALIZES
#endif

// Silences one MSVC Core Guidelines check (by its profile tag, e.g. "f.6") on
// the declaration or statement it precedes, with the reason the check does not
// apply. Other compilers do not run these checks.
#if defined(_MSC_VER) && !defined(__clang__)
#define PRISM_GSL_SUPPRESS(...) [[gsl::suppress(__VA_ARGS__)]]
#define PRISM_SUPPRESS(rule, reason)                                           \
  PRISM_GSL_SUPPRESS(rule, justification : reason)
#else
#define PRISM_SUPPRESS(rule, reason)
#endif

// Keeps an otherwise unreferenced static object (and its constructor's side
// effects) in the binary. MSVC has no equivalent; PRISM_EMIT_ANCHOR covers it.
#if __has_cpp_attribute(gnu::used) && __has_cpp_attribute(gnu::retain)
#define PRISM_KEEP [[gnu::used, gnu::retain]]
#elif __has_cpp_attribute(gnu::used)
#define PRISM_KEEP [[gnu::used]]
#else
#define PRISM_KEEP
#endif
