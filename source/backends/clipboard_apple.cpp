// SPDX-License-Identifier: MPL-2.0

#ifdef __APPLE__
#include "../backend.h"
#include "../backend_catalog.h"
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#elif TARGET_OS_IOS || TARGET_OS_VISION
#import <UIKit/UIKit.h>
#endif
#include <bitset>
#include <string_view>

#if TARGET_OS_OSX || TARGET_OS_IOS || TARGET_OS_VISION
class ClipboardBackend final : public TextToSpeechBackend {
private:
  bool initialized = false;

public:
  static constexpr bool requires_explicit_selection = true;

  [[nodiscard]] std::string_view get_name() const override {
    return "Clipboard";
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    return IS_SUPPORTED_AT_RUNTIME | SUPPORTS_SPEAK | SUPPORTS_OUTPUT |
           REQUIRES_EXPLICIT_SELECTION;
  }

  BackendResult<> initialize() override {
    if (initialized)
      return std::unexpected(BackendError::AlreadyInitialized);
    initialized = true;
    return {};
  }

  BackendResult<> speak(std::string_view text,
                        [[maybe_unused]] bool interrupt) override {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    @autoreleasepool {
      NSString *const string =
          [[NSString alloc] initWithBytes:text.data()
                                   length:text.size()
                                 encoding:NSUTF8StringEncoding];
      if (string == nil)
        return std::unexpected(BackendError::InvalidUtf8);
#if TARGET_OS_OSX
      NSPasteboard *const pasteboard = NSPasteboard.generalPasteboard;
      [pasteboard clearContents];
      if (![pasteboard setString:string forType:NSPasteboardTypeString])
        return std::unexpected(BackendError::SpeakFailure);
#else
      UIPasteboard.generalPasteboard.string = string;
#endif
    }
    return {};
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    return speak(text, interrupt);
  }
};

REGISTER_BACKEND_WITH_ID(ClipboardBackend, Backends::Clipboard, "Clipboard", 0);
#endif
#endif
