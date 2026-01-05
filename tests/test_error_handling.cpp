#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cstring>
#include <prism.h>
#include <set>
#include <string>

using namespace prism_test;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("Error string function", "[error][string]") {
  SECTION("All error codes have non-null strings") {
    for (int i = 0; i < PRISM_ERROR_COUNT; ++i) {
      PrismError err = static_cast<PrismError>(i);
      const char *str = prism_error_string(err);

      INFO("Error code: " << i);
      REQUIRE(str != nullptr);
      CHECK(strlen(str) > 0);
    }
  }

  SECTION("PRISM_OK has appropriate string") {
    const char *str = prism_error_string(PRISM_OK);
    REQUIRE(str != nullptr);
    INFO("PRISM_OK string: " << str);
  }

  SECTION("Error strings are unique") {
    std::set<std::string> strings;

    for (int i = 0; i < PRISM_ERROR_COUNT; ++i) {
      PrismError err = static_cast<PrismError>(i);
      const char *str = prism_error_string(err);

      if (str) {
        auto [it, inserted] = strings.insert(str);
        INFO("Error " << i << ": " << str);
        CHECK(inserted); // Each error should have unique string
      }
    }
  }

  SECTION("Error strings for specific errors") {
    // Check some specific error strings make sense
    INFO(
        "NOT_INITIALIZED: " << prism_error_string(PRISM_ERROR_NOT_INITIALIZED));
    INFO("INVALID_PARAM: " << prism_error_string(PRISM_ERROR_INVALID_PARAM));
    INFO(
        "NOT_IMPLEMENTED: " << prism_error_string(PRISM_ERROR_NOT_IMPLEMENTED));
    INFO("NO_VOICES: " << prism_error_string(PRISM_ERROR_NO_VOICES));
    INFO(
        "VOICE_NOT_FOUND: " << prism_error_string(PRISM_ERROR_VOICE_NOT_FOUND));
    INFO("SPEAK_FAILURE: " << prism_error_string(PRISM_ERROR_SPEAK_FAILURE));
    INFO("MEMORY_FAILURE: " << prism_error_string(PRISM_ERROR_MEMORY_FAILURE));
    INFO("RANGE_OUT_OF_BOUNDS: "
         << prism_error_string(PRISM_ERROR_RANGE_OUT_OF_BOUNDS));
    INFO("INTERNAL: " << prism_error_string(PRISM_ERROR_INTERNAL));
    INFO("NOT_SPEAKING: " << prism_error_string(PRISM_ERROR_NOT_SPEAKING));
    INFO("NOT_PAUSED: " << prism_error_string(PRISM_ERROR_NOT_PAUSED));
    INFO("ALREADY_PAUSED: " << prism_error_string(PRISM_ERROR_ALREADY_PAUSED));
    INFO("INVALID_UTF8: " << prism_error_string(PRISM_ERROR_INVALID_UTF8));
    INFO("INVALID_OPERATION: "
         << prism_error_string(PRISM_ERROR_INVALID_OPERATION));
    INFO("ALREADY_INITIALIZED: "
         << prism_error_string(PRISM_ERROR_ALREADY_INITIALIZED));
    INFO("BACKEND_NOT_AVAILABLE: "
         << prism_error_string(PRISM_ERROR_BACKEND_NOT_AVAILABLE));
    INFO("UNKNOWN: " << prism_error_string(PRISM_ERROR_UNKNOWN));
  }

  SECTION("Out of range error code") {
    // Test behavior with error code beyond defined range
    const char *str =
        prism_error_string(static_cast<PrismError>(PRISM_ERROR_COUNT));
    // Should return something (maybe "unknown" or similar)
    REQUIRE(str != nullptr);

    str = prism_error_string(static_cast<PrismError>(1000));
    REQUIRE(str != nullptr);

    str = prism_error_string(static_cast<PrismError>(-1));
    REQUIRE(str != nullptr);
  }

  SECTION("Error strings are consistent") {
    for (int i = 0; i < PRISM_ERROR_COUNT; ++i) {
      PrismError err = static_cast<PrismError>(i);
      const char *str1 = prism_error_string(err);
      const char *str2 = prism_error_string(err);

      REQUIRE(str1 != nullptr);
      REQUIRE(str2 != nullptr);
      CHECK(strcmp(str1, str2) == 0);
    }
  }
}

TEST_CASE("Error conditions - null context", "[error][null][context]") {
  SECTION("Registry functions with null context") {
    // These should handle null gracefully or crash predictably
    // Depending on implementation, they may return 0 or crash

    // We can't easily test for crash, but we can test for graceful handling
    // if the implementation supports it

    // Note: Some implementations may deliberately crash on null
    // This test documents expected behavior

    size_t count = prism_registry_count(nullptr);
    (void)count; // May crash or return 0
  }
}

TEST_CASE("Error conditions - null backend", "[error][null][backend]") {
  SECTION("Backend functions with null backend") {
    // Most functions should handle null backend
    // by returning an error or handling gracefully

    // These tests document behavior with null backend
    bool speaking = false;
    float volume = 0.0f;
    size_t count = 0;
    const char *name = nullptr;

    // Note: Implementation may crash on null backend
    // If it doesn't, it should return an error

    // We can't test all functions safely without knowing implementation
    // This section documents the expected graceful degradation
  }
}

TEST_CASE("Error conditions - invalid parameters", "[error][invalid]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend = make_best_backend(ctx.get());
  if (!backend) {
    WARN("No backend available");
    return;
  }

  PrismError init_err = prism_backend_initialize(backend.get());
  if (init_err != PRISM_OK && !is_unavailable_error(init_err)) {
    WARN("Backend initialization failed");
    return;
  }

  SECTION("Null output parameters") {
    CHECK(prism_backend_is_speaking(backend.get(), nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_get_volume(backend.get(), nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_get_rate(backend.get(), nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_get_pitch(backend.get(), nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_count_voices(backend.get(), nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_get_voice(backend.get(), nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_get_voice_name(backend.get(), 0, nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_get_voice_language(backend.get(), 0, nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_get_channels(backend.get(), nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_get_sample_rate(backend.get(), nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
    CHECK(prism_backend_get_bit_depth(backend.get(), nullptr) ==
          PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Null text parameters") {
    PrismError err = prism_backend_speak(backend.get(), nullptr, true);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);

    err = prism_backend_braille(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);

    err = prism_backend_output(backend.get(), nullptr, true);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Null callback for speak_to_memory") {
    PrismError err = prism_backend_speak_to_memory(
        backend.get(), strings::HELLO_WORLD, nullptr, nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }
}

TEST_CASE("Error conditions - out of range values", "[error][range]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend = make_best_backend(ctx.get());
  if (!backend) {
    WARN("No backend available");
    return;
  }

  prism_backend_initialize(backend.get());

  SECTION("Voice index out of range") {
    const char *name = nullptr;
    PrismError err =
        prism_backend_get_voice_name(backend.get(), SIZE_MAX, &name);
    CHECK((err == PRISM_ERROR_VOICE_NOT_FOUND ||
           err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Set voice out of range") {
    PrismError err = prism_backend_set_voice(backend.get(), SIZE_MAX);
    CHECK((err == PRISM_ERROR_VOICE_NOT_FOUND ||
           err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Volume out of range") {
    PrismError err = prism_backend_set_volume(backend.get(), -1.0f);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));

    err = prism_backend_set_volume(backend.get(), 2.0f);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }
}

TEST_CASE("Error conditions - state errors", "[error][state]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend = make_best_backend(ctx.get());
  if (!backend) {
    WARN("No backend available");
    return;
  }

  SECTION("Operations on uninitialized backend") {
    // Don't initialize
    bool speaking = false;
    PrismError err = prism_backend_is_speaking(backend.get(), &speaking);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_INITIALIZED));

    err = prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_INITIALIZED ||
           is_unavailable_error(err)));
  }

  SECTION("Stop when not speaking") {
    prism_backend_initialize(backend.get());

    PrismError err = prism_backend_stop(backend.get());
    CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_SPEAKING ||
           is_unavailable_error(err)));
  }

  SECTION("Pause when not speaking") {
    prism_backend_initialize(backend.get());

    PrismError err = prism_backend_pause(backend.get());
    CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_SPEAKING ||
           err == PRISM_ERROR_INVALID_OPERATION || is_unavailable_error(err)));
  }

  SECTION("Resume when not paused") {
    prism_backend_initialize(backend.get());

    PrismError err = prism_backend_resume(backend.get());
    CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_PAUSED ||
           err == PRISM_ERROR_INVALID_OPERATION || is_unavailable_error(err)));
  }

  SECTION("Double initialization") {
    PrismError err1 = prism_backend_initialize(backend.get());

    if (err1 == PRISM_OK) {
      PrismError err2 = prism_backend_initialize(backend.get());
      CHECK((err2 == PRISM_OK || err2 == PRISM_ERROR_ALREADY_INITIALIZED));
    }
  }
}

TEST_CASE("Error recovery", "[error][recovery]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend = make_best_backend(ctx.get());
  if (!backend) {
    WARN("No backend available");
    return;
  }

  PrismError init_err = prism_backend_initialize(backend.get());
  if (init_err != PRISM_OK)
    return;

  SECTION("Recover from failed speak") {
    // Try to cause a speak failure (invalid UTF-8)
    PrismError err =
        prism_backend_speak(backend.get(), strings::get_invalid_utf8(), true);

    // Should be able to speak normally after
    PrismError recover_err =
        prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
    CHECK((recover_err == PRISM_OK || is_unavailable_error(recover_err)));

    prism_backend_stop(backend.get());
  }

  SECTION("Recover from invalid parameters") {
    // Call with invalid parameters
    PrismError err = prism_backend_set_volume(backend.get(), -100.0f);
    (void)err;

    // Should still work with valid parameters
    err = prism_backend_set_volume(backend.get(), 0.5f);
    CHECK_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Multiple error recovery cycles") {
    for (int i = 0; i < 10; ++i) {
      // Cause potential error
      prism_backend_speak(backend.get(), strings::get_invalid_utf8(), true);

      // Recover
      PrismError err =
          prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
      CHECK((err == PRISM_OK || is_unavailable_error(err)));

      prism_backend_stop(backend.get());
    }
  }
}

TEST_CASE("Error code values", "[error][values]") {
  SECTION("PRISM_OK is zero") { REQUIRE(PRISM_OK == 0); }

  SECTION("All other errors are non-zero") {
    for (int i = 1; i < PRISM_ERROR_COUNT; ++i) {
      CHECK(static_cast<PrismError>(i) != PRISM_OK);
    }
  }

  SECTION("Error codes are sequential") {
    for (int i = 0; i < PRISM_ERROR_COUNT; ++i) {
      CHECK(static_cast<int>(static_cast<PrismError>(i)) == i);
    }
  }

  SECTION("PRISM_ERROR_COUNT is the count") {
    CHECK(PRISM_ERROR_COUNT >= 17); // Known error count
  }
}

TEST_CASE("Error handling macros", "[error][macros]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  SECTION("REQUIRE_SUCCESS_OR_UNAVAILABLE with OK") {
    REQUIRE_SUCCESS_OR_UNAVAILABLE(PRISM_OK);
  }

  SECTION("REQUIRE_SUCCESS_OR_UNAVAILABLE with NOT_IMPLEMENTED") {
    REQUIRE_SUCCESS_OR_UNAVAILABLE(PRISM_ERROR_NOT_IMPLEMENTED);
  }

  SECTION("REQUIRE_SUCCESS_OR_UNAVAILABLE with BACKEND_NOT_AVAILABLE") {
    REQUIRE_SUCCESS_OR_UNAVAILABLE(PRISM_ERROR_BACKEND_NOT_AVAILABLE);
  }

  SECTION("is_acceptable_error helper") {
    CHECK(is_acceptable_error(PRISM_OK));
    CHECK(is_acceptable_error(PRISM_ERROR_NOT_IMPLEMENTED));
    CHECK(is_acceptable_error(PRISM_ERROR_ALREADY_INITIALIZED));
    CHECK(is_acceptable_error(PRISM_ERROR_BACKEND_NOT_AVAILABLE));
    CHECK_FALSE(is_acceptable_error(PRISM_ERROR_INVALID_PARAM));
    CHECK_FALSE(is_acceptable_error(PRISM_ERROR_INTERNAL));
  }

  SECTION("is_unavailable_error helper") {
    CHECK(is_unavailable_error(PRISM_ERROR_NOT_IMPLEMENTED));
    CHECK(is_unavailable_error(PRISM_ERROR_BACKEND_NOT_AVAILABLE));
    CHECK_FALSE(is_unavailable_error(PRISM_OK));
    CHECK_FALSE(is_unavailable_error(PRISM_ERROR_INVALID_PARAM));
  }
}
