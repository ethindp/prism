//-------------------------------------------------------------------------------------------------------
// moderncom - Part of HHD Software Belt library
// Copyright (C) 2017 HHD Software Ltd.
// Written by Alexander Bessonov
//
// Licensed under the MIT license. See LICENSE file in the project root for full
// license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

#include <exception>

////////////////////////////
// The following code adapted from Andrei Alexandrescu talk on cppcon 2015

namespace belt::details {
class UncaughtExceptionsCounter {
  int exceptions_on_enter{std::uncaught_exceptions()};

public:
  bool has_new_exceptions() const noexcept {
    return std::uncaught_exceptions() > exceptions_on_enter;
  }
};

template <class F, bool execute_on_exception> class scope_guard {
  F f;
  UncaughtExceptionsCounter counter;

public:
  explicit scope_guard(const F &f) noexcept : f{f} {}

  explicit scope_guard(F &&f) noexcept : f{std::move(f)} {}

  ~scope_guard() noexcept(execute_on_exception) {
    if (execute_on_exception == counter.has_new_exceptions())
      f();
  }
};

template <class F> class scope_exit {
  F f;

public:
  explicit scope_exit(const F &f) noexcept : f{f} {}

  explicit scope_exit(F &&f) noexcept : f{std::move(f)} {}

  ~scope_exit() noexcept { f(); }
};

template <class F> class scope_exit_cancellable {
  F f;
  bool cancelled{false};

public:
  explicit scope_exit_cancellable(const F &f) noexcept : f{f} {}

  explicit scope_exit_cancellable(F &&f) noexcept : f{std::move(f)} {}

  void cancel() { cancelled = true; }

  ~scope_exit_cancellable() noexcept {
    if (!cancelled)
      f();
  }
};

enum class ScopeGuardOnFail {};

template <class F>
inline scope_guard<std::decay_t<F>, true> operator+(ScopeGuardOnFail,
                                                    F &&f) noexcept {
  return scope_guard<std::decay_t<F>, true>(std::forward<F>(f));
}

enum class ScopeGuardOnSuccess {};

template <class F>
inline scope_guard<std::decay_t<F>, false> operator+(ScopeGuardOnSuccess,
                                                     F &&f) noexcept {
  return scope_guard<std::decay_t<F>, false>(std::forward<F>(f));
}

enum class ScopeGuardOnExit {};

template <class F>
inline scope_exit<std::decay_t<F>> operator+(ScopeGuardOnExit, F &&f) noexcept {
  return scope_exit<std::decay_t<F>>(std::forward<F>(f));
}

enum class ScopeGuardOnExitCancellable {};

template <class F>
inline scope_exit_cancellable<std::decay_t<F>>
operator+(ScopeGuardOnExitCancellable, F &&f) noexcept {
  return scope_exit_cancellable<std::decay_t<F>>(std::forward<F>(f));
}
} // namespace belt::details

#define PP_CAT(a, b) a##b

// Alexandrescu
#define ANONYMOUS_VARIABLE(x) PP_CAT(x, __COUNTER__)

#define SCOPE_FAIL                                                             \
  auto ANONYMOUS_VARIABLE(SCOPE_FAIL_STATE) =                                  \
      ::belt::details::ScopeGuardOnFail() + [&]() noexcept // end of macro

#define SCOPE_SUCCESS                                                          \
  auto ANONYMOUS_VARIABLE(SCOPE_SUCCESS_STATE) =                               \
      ::belt::details::ScopeGuardOnSuccess() + [&]() // end of macro

#define SCOPE_EXIT                                                             \
  auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) =                                  \
      ::belt::details::ScopeGuardOnExit() + [&]() noexcept // end of macro

#define SCOPE_EXIT_CANCELLABLE(name)                                           \
  auto name =                                                                  \
      ::belt::details::ScopeGuardOnExitCancellable() + [&]() noexcept // end of
                                                                      // macro
