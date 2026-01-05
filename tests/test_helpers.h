#pragma once
#ifndef PRISM_TEST_HELPERS_H
#define PRISM_TEST_HELPERS_H

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <prism.h>
#include <string>
#include <thread>
#include <vector>

namespace prism_test {

// Custom deleters for smart pointer management
struct ContextDeleter {
  void operator()(PrismContext *ctx) const {
    if (ctx) {
      prism_shutdown(ctx);
    }
  }
};

struct BackendDeleter {
  void operator()(PrismBackend *backend) const {
    if (backend) {
      prism_backend_free(backend);
    }
  }
};

// Smart pointer types for automatic cleanup
using ContextPtr = std::unique_ptr<PrismContext, ContextDeleter>;
using BackendPtr = std::unique_ptr<PrismBackend, BackendDeleter>;

// Helper to create a managed context
inline ContextPtr make_context() { return ContextPtr(prism_init()); }

// Helper to create a managed backend
inline BackendPtr make_backend(PrismContext *ctx, PrismBackendId id) {
  return BackendPtr(prism_registry_create(ctx, id));
}

inline BackendPtr make_best_backend(PrismContext *ctx) {
  return BackendPtr(prism_registry_create_best(ctx));
}

// Check if an error is acceptable (platform-specific or expected)
inline bool is_acceptable_error(PrismError err) {
  return err == PRISM_OK || err == PRISM_ERROR_NOT_IMPLEMENTED ||
         err == PRISM_ERROR_ALREADY_INITIALIZED ||
         err == PRISM_ERROR_BACKEND_NOT_AVAILABLE;
}

// Check if error indicates unavailable functionality
inline bool is_unavailable_error(PrismError err) {
  return err == PRISM_ERROR_NOT_IMPLEMENTED ||
         err == PRISM_ERROR_BACKEND_NOT_AVAILABLE;
}

// Helper macro for checking errors that may be platform-specific
#define REQUIRE_SUCCESS_OR_UNAVAILABLE(err)                                    \
  do {                                                                         \
    PrismError _err = (err);                                                   \
    INFO("Error code: " << static_cast<int>(_err) << " ("                      \
                        << prism_error_string(_err) << ")");                   \
    REQUIRE((_err == PRISM_OK || prism_test::is_unavailable_error(_err)));     \
  } while (0)

#define REQUIRE_SUCCESS(err)                                                   \
  do {                                                                         \
    PrismError _err = (err);                                                   \
    INFO("Error code: " << static_cast<int>(_err) << " ("                      \
                        << prism_error_string(_err) << ")");                   \
    REQUIRE(_err == PRISM_OK);                                                 \
  } while (0)

#define CHECK_SUCCESS_OR_UNAVAILABLE(err)                                      \
  do {                                                                         \
    PrismError _err = (err);                                                   \
    INFO("Error code: " << static_cast<int>(_err) << " ("                      \
                        << prism_error_string(_err) << ")");                   \
    CHECK((_err == PRISM_OK || prism_test::is_unavailable_error(_err)));       \
  } while (0)

// Audio callback test helper
struct AudioCallbackData {
  std::vector<float> samples;
  size_t channels = 0;
  size_t sample_rate = 0;
  size_t bit_depth = 0;
  size_t callback_count = 0;
  std::atomic<bool> completed{false};

  void reset() {
    samples.clear();
    channels = 0;
    sample_rate = 0;
    bit_depth = 0;
    callback_count = 0;
    completed = false;
  }
};

inline void test_audio_callback(void *userdata, const float *samples,
                                size_t num_samples, size_t channels,
                                size_t sample_rate) {
  auto *data = static_cast<AudioCallbackData *>(userdata);
  if (data) {
    data->channels = channels;
    data->sample_rate = sample_rate;
    data->callback_count++;
    if (samples && num_samples > 0) {
      data->samples.insert(data->samples.end(), samples, samples + num_samples);
    }
  }
}

// Test string constants
namespace strings {
constexpr const char *HELLO_WORLD = "Hello, World!";
constexpr const char *EMPTY = "";
constexpr const char *LONG_TEXT =
    "This is a significantly longer piece of text that is designed to test "
    "the text-to-speech engine's ability to handle longer passages. It "
    "contains "
    "multiple sentences and should provide a more comprehensive test of the "
    "speech synthesis capabilities. The quick brown fox jumps over the lazy "
    "dog. "
    "Pack my box with five dozen liquor jugs. How vexingly quick daft zebras "
    "jump!";
constexpr const char *UNICODE_TEXT =
    "Hello, \xe4\xb8\x96\xe7\x95\x8c! "
    "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82 "
    "\xd0\xbc\xd0\xb8\xd1\x80! \xd9\x85\xd8\xb1\xd8\xad\xd8\xa8\xd8\xa7 "
    "\xd8\xa8\xd8\xa7\xd9\x84\xd8\xb9\xd8\xa7\xd9\x84\xd9\x85";
constexpr const char *NUMBERS = "1 2 3 4 5 6 7 8 9 10";
constexpr const char *PUNCTUATION = "Hello! How are you? I'm fine, thanks.";
constexpr const char *SPECIAL_CHARS = "Test <tag> & \"quotes\" 'apostrophe'";
constexpr const char *NEWLINES = "Line one.\nLine two.\nLine three.";
constexpr const char *TABS = "Column1\tColumn2\tColumn3";
constexpr const char *WHITESPACE = "   spaces   and   whitespace   ";

// Invalid UTF-8 sequences
inline const char *get_invalid_utf8() {
  static const char invalid[] = {'\x80', '\x81', '\x82', '\0'};
  return invalid;
}

inline const char *get_truncated_utf8() {
  // Truncated multi-byte sequence
  static const char truncated[] = {
      '\xC2', '\0'}; // Should be followed by continuation byte
  return truncated;
}

inline const char *get_overlong_utf8() {
  // Overlong encoding of '/'
  static const char overlong[] = {'\xC0', '\xAF', '\0'};
  return overlong;
}
} // namespace strings

// Backend info helper
struct BackendInfo {
  PrismBackendId id;
  std::string name;
  int priority;
  bool exists;
};

inline std::vector<BackendInfo> get_all_backends(PrismContext *ctx) {
  std::vector<BackendInfo> backends;
  size_t count = prism_registry_count(ctx);

  for (size_t i = 0; i < count; ++i) {
    BackendInfo info;
    info.id = prism_registry_id_at(ctx, i);
    const char *name = prism_registry_name(ctx, info.id);
    info.name = name ? name : "";
    info.priority = prism_registry_priority(ctx, info.id);
    info.exists = prism_registry_exists(ctx, info.id);
    backends.push_back(info);
  }

  return backends;
}

// Known backend IDs for testing
constexpr PrismBackendId KNOWN_BACKEND_IDS[] = {
    PRISM_BACKEND_SAPI,       PRISM_BACKEND_AV_SPEECH,
    PRISM_BACKEND_VOICE_OVER, PRISM_BACKEND_SPEECH_DISPATCHER,
    PRISM_BACKEND_NVDA,       PRISM_BACKEND_JAWS,
    PRISM_BACKEND_ONE_CORE,   PRISM_BACKEND_ORCA};

constexpr size_t NUM_KNOWN_BACKENDS =
    sizeof(KNOWN_BACKEND_IDS) / sizeof(KNOWN_BACKEND_IDS[0]);

// Platform detection helpers
inline bool is_windows() {
#ifdef _WIN32
  return true;
#else
  return false;
#endif
}

inline bool is_macos() {
#ifdef __APPLE__
  return true;
#else
  return false;
#endif
}

inline bool is_linux() {
#ifdef __linux__
  return true;
#else
  return false;
#endif
}

// RAII helper for temporary backend initialization
class ScopedBackendInit {
public:
  ScopedBackendInit(PrismBackend *backend)
      : backend_(backend), initialized_(false) {
    if (backend_) {
      PrismError err = prism_backend_initialize(backend_);
      initialized_ = (err == PRISM_OK);
    }
  }

  bool is_initialized() const { return initialized_; }
  PrismBackend *get() const { return backend_; }

private:
  PrismBackend *backend_;
  bool initialized_;
};

// Floating point comparison helpers
inline bool float_near(float a, float b, float epsilon = 0.001f) {
  return std::abs(a - b) < epsilon;
}

// Rate/pitch/volume range constants
constexpr float MIN_RATE = 0.0f;
constexpr float MAX_RATE = 1.0f;
constexpr float DEFAULT_RATE = 0.5f;

constexpr float MIN_PITCH = 0.0f;
constexpr float MAX_PITCH = 1.0f;
constexpr float DEFAULT_PITCH = 0.5f;

constexpr float MIN_VOLUME = 0.0f;
constexpr float MAX_VOLUME = 1.0f;
constexpr float DEFAULT_VOLUME = 1.0f;

} // namespace prism_test

#endif // PRISM_TEST_HELPERS_H
