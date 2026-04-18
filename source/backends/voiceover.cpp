// SPDX-License-Identifier: MPL-2.0

#include "../simdutf.h"
#include "backend.h"
#include "backend_registry.h"
#ifdef __OBJC__
#ifdef __APPLE__
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif
#include <atomic>
#include <bitset>
#include <string_view>

#if TARGET_OS_OSX
inline constexpr bool kIsMacOS = true;
inline constexpr bool kIsVision = false;
inline constexpr bool kIsTV = false;
inline constexpr bool kIsWatch = false;
#elif TARGET_OS_VISION
inline constexpr bool kIsMacOS = false;
inline constexpr bool kIsVision = true;
inline constexpr bool kIsTV = false;
inline constexpr bool kIsWatch = false;
#elif TARGET_OS_TV
inline constexpr bool kIsMacOS = false;
inline constexpr bool kIsVision = false;
inline constexpr bool kIsTV = true;
inline constexpr bool kIsWatch = false;
#elif TARGET_OS_WATCH
inline constexpr bool kIsMacOS = false;
inline constexpr bool kIsVision = false;
inline constexpr bool kIsTV = false;
inline constexpr bool kIsWatch = true;
#else
inline constexpr bool kIsMacOS = false;
inline constexpr bool kIsVision = false;
inline constexpr bool kIsTV = false;
inline constexpr bool kIsWatch = false;
#endif

consteval std::string_view backend_display_name() {
  if constexpr (kIsMacOS)
    return "VoiceOver (macOS)";
  if constexpr (kIsVision)
    return "VoiceOver (visionOS)";
  if constexpr (kIsTV)
    return "VoiceOver (tvOS)";
  if constexpr (kIsWatch)
    return "VoiceOver (watchOS)";
  return "VoiceOver (iOS)";
}

static inline void sync_on_main(dispatch_block_t block) {
  if ([NSThread isMainThread]) {
    block();
  } else {
    dispatch_sync(dispatch_get_main_queue(), block);
  }
}

static inline bool is_voiceover_active() {
#if TARGET_OS_OSX
  return [[NSWorkspace sharedWorkspace] isVoiceOverEnabled];
#else
  return UIAccessibilityIsVoiceOverRunning();
#endif
}

#if TARGET_OS_OSX
static NSWindow *pick_best_window() {
  NSApplication *app = [NSApplication sharedApplication];
  if (NSWindow *kw = app.keyWindow; kw && kw.isVisible)
    return kw;
  if (NSWindow *mw = app.mainWindow; mw && mw.isVisible)
    return mw;
  for (NSWindow *w in app.orderedWindows) {
    if (w.isVisible && !w.isMiniaturized && w.level == NSNormalWindowLevel) {
      return w;
    }
  }
  for (NSWindow *w in app.windows) {
    if (w.contentView != nil)
      return w;
  }
  return app.windows.firstObject;
}
#endif

class VoiceOverBackend final : public TextToSpeechBackend {
private:
  std::atomic_flag initialized;
#if TARGET_OS_OSX
  NSMutableString *pending_text{nullptr};
  dispatch_block_t debounce_block{nullptr};
  __weak NSWindow *cached_window{nullptr};
  static constexpr double kDebounceDelay = 0.015;
#else
  NSMutableArray<NSString *> *queue{nullptr};
  bool is_speaking_flag{false};
  id observer{nullptr};
#endif

public:
  VoiceOverBackend() {
    if constexpr (kIsMacOS) {
#if TARGET_OS_OSX
      pending_text = [NSMutableString string];
#endif
    } else {
#if !TARGET_OS_OSX
      queue = [NSMutableArray array];
#endif
    }
  }

  ~VoiceOverBackend() override {
    if (initialized.test()) {
      sync_on_main(^{
        shutdown_impl();
      });
    }
  }

  [[nodiscard]] std::string_view get_name() const override {
    return backend_display_name();
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    std::bitset<64> features;
    features |=
        SUPPORTS_SPEAK | SUPPORTS_OUTPUT | SUPPORTS_IS_SPEAKING | SUPPORTS_STOP;
    if (is_voiceover_active()) {
      features |= IS_SUPPORTED_AT_RUNTIME;
    }
    return features;
  }

  BackendResult<> initialize() override {
    if (initialized.test())
      return std::unexpected(BackendError::AlreadyInitialized);
    __block BackendResult<> result = {};
    sync_on_main(^{
      if (!is_voiceover_active()) {
        result = std::unexpected(BackendError::BackendNotAvailable);
        return;
      }
#if TARGET_OS_OSX
      NSWindow *w = pick_best_window();
      if (!w) {
        result = std::unexpected(BackendError::BackendNotAvailable);
        return;
      }
      cached_window = w;
      [pending_text setString:@""];
      cancel_debounce();
      dispatch_async(dispatch_get_main_queue(), ^{
        NSAccessibilityPostNotification(
            w, NSAccessibilityWindowCreatedNotification);
        NSAccessibilityPostNotification(
            w, NSAccessibilityFocusedWindowChangedNotification);
      });
#else
      VoiceOverBackend *self_ptr = this;
      observer = [[NSNotificationCenter defaultCenter]
          addObserverForName:UIAccessibilityAnnouncementDidFinishNotification
                      object:nil
                       queue:[NSOperationQueue mainQueue]
                  usingBlock:^(NSNotification *_Nonnull) {
                    if (self_ptr->initialized.test()) {
                      self_ptr->is_speaking_flag = false;
                      self_ptr->pump_if_needed();
                    }
                  }];
      is_speaking_flag = false;
      [queue removeAllObjects];
      dispatch_async(dispatch_get_main_queue(), ^{
        UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification, nil);
      });
#endif
    });
    if (!result)
      return result;
    initialized.test_and_set();
    return {};
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    if (!simdutf::validate_utf8(text.data(), text.size()))
      return std::unexpected(BackendError::InvalidUtf8);
    NSString *ns_text = [[NSString alloc] initWithBytes:text.data()
                                                 length:text.size()
                                               encoding:NSUTF8StringEncoding];
    if (!ns_text)
      return std::unexpected(BackendError::InvalidUtf8);
    __block BackendResult<> result = {};
    sync_on_main(^{
      if (!is_voiceover_active()) {
        result = std::unexpected(BackendError::BackendNotAvailable);
        return;
      }
#if TARGET_OS_OSX
      if (!cached_window || cached_window.contentView == nil) {
        cached_window = pick_best_window();
      }
      if (!cached_window) {
        result = std::unexpected(BackendError::BackendNotAvailable);
        return;
      }
      if (interrupt || pending_text.length == 0) {
        [pending_text setString:ns_text];
      } else {
        [pending_text appendString:@" . "];
        [pending_text appendString:ns_text];
      }
      schedule_debounced_announcement();
#else
      if (interrupt) {
        [queue removeAllObjects];
        is_speaking_flag = false;
      }
      [queue addObject:ns_text];
      pump_if_needed();
#endif
    });
    return result;
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }

  BackendResult<bool> is_speaking() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    __block bool speaking = false;
    sync_on_main(^{
#if TARGET_OS_OSX
      speaking = pending_text.length > 0 || debounce_block != nullptr;
#else
      speaking = is_speaking_flag || queue.count > 0;
#endif
    });
    return speaking;
  }

  BackendResult<> stop() override {
    if (!initialized.test())
      return std::unexpected(BackendError::NotInitialized);
    sync_on_main(^{
#if TARGET_OS_OSX
      cancel_debounce();
      [pending_text setString:@""];
#else
      [queue removeAllObjects];
      is_speaking_flag = false;
#endif
    });
    return {};
  }

private:
  void shutdown_impl() {
#if TARGET_OS_OSX
    cancel_debounce();
    [pending_text setString:@""];
    cached_window = nullptr;
#else
    if (observer) {
      [[NSNotificationCenter defaultCenter] removeObserver:observer];
      observer = nullptr;
    }
    [queue removeAllObjects];
    is_speaking_flag = false;
#endif
  }

#if TARGET_OS_OSX
  void cancel_debounce() {
    if (debounce_block) {
      dispatch_block_cancel(debounce_block);
      debounce_block = nullptr;
    }
  }

  void schedule_debounced_announcement() {
    cancel_debounce();
    VoiceOverBackend *self_ptr = this;
    debounce_block =
        dispatch_block_create(static_cast<dispatch_block_flags_t>(0), ^{
          self_ptr->debounce_block = nullptr;
          self_ptr->fire_announcement_now();
        });
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      static_cast<int64_t>(kDebounceDelay * NSEC_PER_SEC)),
        dispatch_get_main_queue(), debounce_block);
  }

  void fire_announcement_now() {
    if (!initialized.test())
      return;
    if (![[NSWorkspace sharedWorkspace] isVoiceOverEnabled])
      return;
    if (pending_text.length == 0)
      return;
    NSString *text_to_speak = [pending_text copy];
    [pending_text setString:@""];
    NSWindow *w = cached_window ?: pick_best_window();
    if (!w)
      return;
    cached_window = w;
    NSAccessibilityPostNotificationWithUserInfo(
        w, NSAccessibilityAnnouncementRequestedNotification, @{
          NSAccessibilityAnnouncementKey : text_to_speak,
          NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
        });
  }
#else
  void pump_if_needed() {
    if (!initialized.test())
      return;
    if (is_speaking_flag)
      return;
    if (queue.count == 0)
      return;
    NSString *next = queue.firstObject;
    [queue removeObjectAtIndex:0];
    is_speaking_flag = true;
    dispatch_async(dispatch_get_main_queue(), ^{
      UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                      next);
    });
  }
#endif
};

REGISTER_BACKEND_WITH_ID(VoiceOverBackend, Backends::VoiceOver, "VoiceOver",
                         102);
#endif
#endif