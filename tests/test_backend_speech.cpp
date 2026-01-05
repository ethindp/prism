#include "test_helpers.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <prism.h>
#include <thread>

using namespace prism_test;
using namespace std::chrono_literals;

// Helper to get an initialized backend
static PrismBackend *get_initialized_backend(PrismContext *ctx) {
  PrismBackend *backend = prism_registry_create_best(ctx);
  if (backend) {
    PrismError err = prism_backend_initialize(backend);
    if (err != PRISM_OK && !is_unavailable_error(err)) {
      prism_backend_free(backend);
      return nullptr;
    }
    if (is_unavailable_error(err)) {
      prism_backend_free(backend);
      return nullptr;
    }
  }
  return backend;
}

TEST_CASE("Backend speak", "[backend][speech][speak]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping speak tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Speak simple text") {
    PrismError err =
        prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak with interrupt false") {
    PrismError err =
        prism_backend_speak(backend.get(), strings::HELLO_WORLD, false);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak empty string") {
    PrismError err = prism_backend_speak(backend.get(), strings::EMPTY, true);
    // Empty string might succeed or return an error
    CHECK((err == PRISM_OK || err == PRISM_ERROR_INVALID_PARAM ||
           is_unavailable_error(err)));
  }

  SECTION("Speak long text") {
    PrismError err =
        prism_backend_speak(backend.get(), strings::LONG_TEXT, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak unicode text") {
    PrismError err =
        prism_backend_speak(backend.get(), strings::UNICODE_TEXT, true);
    // Unicode may or may not be supported
    CHECK((err == PRISM_OK || err == PRISM_ERROR_INVALID_UTF8 ||
           is_unavailable_error(err)));
  }

  SECTION("Speak numbers") {
    PrismError err = prism_backend_speak(backend.get(), strings::NUMBERS, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak punctuation") {
    PrismError err =
        prism_backend_speak(backend.get(), strings::PUNCTUATION, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak special characters") {
    PrismError err =
        prism_backend_speak(backend.get(), strings::SPECIAL_CHARS, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak text with newlines") {
    PrismError err =
        prism_backend_speak(backend.get(), strings::NEWLINES, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak text with tabs") {
    PrismError err = prism_backend_speak(backend.get(), strings::TABS, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak text with extra whitespace") {
    PrismError err =
        prism_backend_speak(backend.get(), strings::WHITESPACE, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Multiple speak calls") {
    for (int i = 0; i < 5; ++i) {
      PrismError err =
          prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
      REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Speak with interrupt between calls") {
    PrismError err1 =
        prism_backend_speak(backend.get(), strings::LONG_TEXT, false);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err1);

    // Short delay
    std::this_thread::sleep_for(10ms);

    // Interrupt with new text
    PrismError err2 =
        prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err2);
  }

  SECTION("Speak very short text") {
    PrismError err = prism_backend_speak(backend.get(), "A", true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak single word") {
    PrismError err = prism_backend_speak(backend.get(), "Hello", true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }
}

TEST_CASE("Backend speak to memory", "[backend][speech][memory]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping speak_to_memory tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Speak to memory with valid callback") {
    AudioCallbackData data;

    PrismError err = prism_backend_speak_to_memory(
        backend.get(), strings::HELLO_WORLD, test_audio_callback, &data);

    if (err == PRISM_OK) {
      // Give it some time to generate audio
      std::this_thread::sleep_for(500ms);

      // Callback should have been called at least once if successful
      // (though this depends on the backend)
      INFO("Callback count: " << data.callback_count);
      INFO("Samples received: " << data.samples.size());
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Speak to memory with null userdata") {
    PrismError err = prism_backend_speak_to_memory(
        backend.get(), strings::HELLO_WORLD, test_audio_callback, nullptr);

    CHECK_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Speak to memory with empty text") {
    AudioCallbackData data;

    PrismError err = prism_backend_speak_to_memory(
        backend.get(), strings::EMPTY, test_audio_callback, &data);

    CHECK((err == PRISM_OK || err == PRISM_ERROR_INVALID_PARAM ||
           is_unavailable_error(err)));
  }

  SECTION("Speak to memory with long text") {
    AudioCallbackData data;

    PrismError err = prism_backend_speak_to_memory(
        backend.get(), strings::LONG_TEXT, test_audio_callback, &data);

    CHECK_SUCCESS_OR_UNAVAILABLE(err);
  }
}

TEST_CASE("Backend braille", "[backend][speech][braille]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping braille tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Braille simple text") {
    PrismError err = prism_backend_braille(backend.get(), strings::HELLO_WORLD);
    // Braille is often not implemented
    CHECK((err == PRISM_OK || is_unavailable_error(err)));
  }

  SECTION("Braille empty text") {
    PrismError err = prism_backend_braille(backend.get(), strings::EMPTY);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_INVALID_PARAM ||
           is_unavailable_error(err)));
  }
}

TEST_CASE("Backend output", "[backend][speech][output]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping output tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Output simple text with interrupt") {
    PrismError err =
        prism_backend_output(backend.get(), strings::HELLO_WORLD, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Output simple text without interrupt") {
    PrismError err =
        prism_backend_output(backend.get(), strings::HELLO_WORLD, false);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Output empty text") {
    PrismError err = prism_backend_output(backend.get(), strings::EMPTY, true);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_INVALID_PARAM ||
           is_unavailable_error(err)));
  }

  SECTION("Output long text") {
    PrismError err =
        prism_backend_output(backend.get(), strings::LONG_TEXT, true);
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Multiple output calls") {
    for (int i = 0; i < 3; ++i) {
      PrismError err =
          prism_backend_output(backend.get(), strings::HELLO_WORLD, true);
      REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
    }
  }
}

TEST_CASE("Backend stop", "[backend][speech][stop]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping stop tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Stop when not speaking") {
    PrismError err = prism_backend_stop(backend.get());
    // Stop when not speaking should be OK or return NOT_SPEAKING
    CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_SPEAKING ||
           is_unavailable_error(err)));
  }

  SECTION("Stop after starting speech") {
    PrismError speak_err =
        prism_backend_speak(backend.get(), strings::LONG_TEXT, false);

    if (speak_err == PRISM_OK) {
      // Give it a moment to start
      std::this_thread::sleep_for(10ms);

      PrismError stop_err = prism_backend_stop(backend.get());
      REQUIRE_SUCCESS_OR_UNAVAILABLE(stop_err);
    }
  }

  SECTION("Multiple stop calls") {
    for (int i = 0; i < 5; ++i) {
      PrismError err = prism_backend_stop(backend.get());
      CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_SPEAKING ||
             is_unavailable_error(err)));
    }
  }

  SECTION("Stop after stop") {
    PrismError err1 = prism_backend_stop(backend.get());
    (void)err1;

    PrismError err2 = prism_backend_stop(backend.get());
    CHECK((err2 == PRISM_OK || err2 == PRISM_ERROR_NOT_SPEAKING ||
           is_unavailable_error(err2)));
  }
}

TEST_CASE("Backend pause and resume", "[backend][speech][pause]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping pause/resume tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Pause when not speaking") {
    PrismError err = prism_backend_pause(backend.get());
    // Pause when not speaking should fail
    CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_SPEAKING ||
           err == PRISM_ERROR_INVALID_OPERATION || is_unavailable_error(err)));
  }

  SECTION("Resume when not paused") {
    PrismError err = prism_backend_resume(backend.get());
    CHECK((err == PRISM_OK || err == PRISM_ERROR_NOT_PAUSED ||
           err == PRISM_ERROR_INVALID_OPERATION || is_unavailable_error(err)));
  }

  SECTION("Pause during speech") {
    PrismError speak_err =
        prism_backend_speak(backend.get(), strings::LONG_TEXT, false);

    if (speak_err == PRISM_OK) {
      std::this_thread::sleep_for(50ms);

      PrismError pause_err = prism_backend_pause(backend.get());
      CHECK((pause_err == PRISM_OK || is_unavailable_error(pause_err) ||
             pause_err == PRISM_ERROR_NOT_SPEAKING));

      if (pause_err == PRISM_OK) {
        PrismError resume_err = prism_backend_resume(backend.get());
        CHECK((resume_err == PRISM_OK || is_unavailable_error(resume_err)));
      }

      prism_backend_stop(backend.get());
    }
  }

  SECTION("Double pause") {
    PrismError speak_err =
        prism_backend_speak(backend.get(), strings::LONG_TEXT, false);

    if (speak_err == PRISM_OK) {
      std::this_thread::sleep_for(50ms);

      PrismError pause1 = prism_backend_pause(backend.get());
      if (pause1 == PRISM_OK) {
        PrismError pause2 = prism_backend_pause(backend.get());
        CHECK((pause2 == PRISM_OK || pause2 == PRISM_ERROR_ALREADY_PAUSED ||
               is_unavailable_error(pause2)));

        prism_backend_resume(backend.get());
      }

      prism_backend_stop(backend.get());
    }
  }

  SECTION("Resume without pause") {
    PrismError speak_err =
        prism_backend_speak(backend.get(), strings::LONG_TEXT, false);

    if (speak_err == PRISM_OK) {
      std::this_thread::sleep_for(10ms);

      PrismError resume_err = prism_backend_resume(backend.get());
      CHECK((resume_err == PRISM_OK || resume_err == PRISM_ERROR_NOT_PAUSED ||
             is_unavailable_error(resume_err)));

      prism_backend_stop(backend.get());
    }
  }
}

TEST_CASE("Backend is speaking", "[backend][speech][is_speaking]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping is_speaking tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Not speaking initially") {
    bool speaking = true;
    PrismError err = prism_backend_is_speaking(backend.get(), &speaking);

    if (err == PRISM_OK) {
      CHECK_FALSE(speaking);
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Speaking during playback") {
    bool speaking = false;

    PrismError speak_err =
        prism_backend_speak(backend.get(), strings::LONG_TEXT, false);

    if (speak_err == PRISM_OK) {
      std::this_thread::sleep_for(50ms);

      PrismError err = prism_backend_is_speaking(backend.get(), &speaking);
      if (err == PRISM_OK) {
        // Might still be speaking
        INFO("Speaking: " << speaking);
      }

      prism_backend_stop(backend.get());
    }
  }

  SECTION("Not speaking after stop") {
    PrismError speak_err =
        prism_backend_speak(backend.get(), strings::LONG_TEXT, false);

    if (speak_err == PRISM_OK) {
      prism_backend_stop(backend.get());

      // Give it time to stop
      std::this_thread::sleep_for(100ms);

      bool speaking = true;
      PrismError err = prism_backend_is_speaking(backend.get(), &speaking);
      if (err == PRISM_OK) {
        CHECK_FALSE(speaking);
      }
    }
  }

  SECTION("is_speaking with null output") {
    // This should either handle gracefully or fail
    // Passing null should be caught
    PrismError err = prism_backend_is_speaking(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }
}

TEST_CASE("Backend speech stress test", "[backend][speech][stress]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping stress tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Rapid speak/stop cycles") {
    for (int i = 0; i < 20; ++i) {
      PrismError speak_err =
          prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
      (void)speak_err;

      PrismError stop_err = prism_backend_stop(backend.get());
      (void)stop_err;
    }
  }

  SECTION("Many speak calls with interrupt") {
    for (int i = 0; i < 50; ++i) {
      PrismError err =
          prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
      (void)err;
    }

    prism_backend_stop(backend.get());
  }

  SECTION("Interleaved operations") {
    for (int i = 0; i < 10; ++i) {
      prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);

      bool speaking;
      prism_backend_is_speaking(backend.get(), &speaking);

      prism_backend_pause(backend.get());
      prism_backend_resume(backend.get());
      prism_backend_stop(backend.get());
    }
  }
}
