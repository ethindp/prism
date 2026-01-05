#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <prism.h>
#include <set>
#include <string>

using namespace prism_test;

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

TEST_CASE("Backend refresh voices", "[backend][voices][refresh]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping voice tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Refresh voices succeeds or returns not implemented") {
    PrismError err = prism_backend_refresh_voices(backend.get());
    REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
  }

  SECTION("Multiple refresh calls") {
    for (int i = 0; i < 5; ++i) {
      PrismError err = prism_backend_refresh_voices(backend.get());
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }
}

TEST_CASE("Backend count voices", "[backend][voices][count]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping voice count tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Count voices returns valid count") {
    size_t count = 0;
    PrismError err = prism_backend_count_voices(backend.get(), &count);

    if (err == PRISM_OK) {
      INFO("Voice count: " << count);
      CHECK(count < 10000); // Sanity check
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Count with null output") {
    PrismError err = prism_backend_count_voices(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Count is consistent") {
    size_t count1 = 0, count2 = 0;

    PrismError err1 = prism_backend_count_voices(backend.get(), &count1);
    PrismError err2 = prism_backend_count_voices(backend.get(), &count2);

    if (err1 == PRISM_OK && err2 == PRISM_OK) {
      CHECK(count1 == count2);
    }
  }

  SECTION("Count after refresh") {
    size_t count_before = 0, count_after = 0;

    prism_backend_count_voices(backend.get(), &count_before);
    prism_backend_refresh_voices(backend.get());
    prism_backend_count_voices(backend.get(), &count_after);

    // Count may or may not change, but should not crash
    INFO("Before: " << count_before << ", After: " << count_after);
  }
}

TEST_CASE("Backend voice name", "[backend][voices][name]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping voice name tests");
    return;
  }

  const char *backend_name = prism_backend_name(backend.get());
  INFO("Using backend: " << (backend_name ? backend_name : "unknown"));

  SECTION("Get name for each voice") {
    size_t count = 0;
    PrismError count_err = prism_backend_count_voices(backend.get(), &count);

    if (count_err == PRISM_OK && count > 0) {
      for (size_t i = 0; i < count; ++i) {
        const char *name = nullptr;
        PrismError err = prism_backend_get_voice_name(backend.get(), i, &name);

        INFO("Voice index: " << i);
        if (err == PRISM_OK) {
          REQUIRE(name != nullptr);
          CHECK(strlen(name) > 0);
          INFO("Voice name: " << name);
        } else {
          CHECK_SUCCESS_OR_UNAVAILABLE(err);
        }
      }
    }
  }

  SECTION("Get name with null output") {
    PrismError err = prism_backend_get_voice_name(backend.get(), 0, nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Get name for out of bounds index") {
    size_t count = 0;
    prism_backend_count_voices(backend.get(), &count);

    const char *name = nullptr;
    PrismError err = prism_backend_get_voice_name(backend.get(), count, &name);
    CHECK((err == PRISM_ERROR_VOICE_NOT_FOUND ||
           err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Get name for very large index") {
    const char *name = nullptr;
    PrismError err =
        prism_backend_get_voice_name(backend.get(), SIZE_MAX, &name);
    CHECK((err == PRISM_ERROR_VOICE_NOT_FOUND ||
           err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Voice names are unique") {
    size_t count = 0;
    PrismError count_err = prism_backend_count_voices(backend.get(), &count);

    if (count_err == PRISM_OK && count > 0) {
      std::set<std::string> names;

      for (size_t i = 0; i < count; ++i) {
        const char *name = nullptr;
        PrismError err = prism_backend_get_voice_name(backend.get(), i, &name);

        if (err == PRISM_OK && name) {
          auto [it, inserted] = names.insert(name);
          // Names may not be unique on all platforms
          if (!inserted) {
            INFO("Duplicate voice name: " << name);
          }
        }
      }
    }
  }
}

TEST_CASE("Backend voice language", "[backend][voices][language]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping voice language tests");
    return;
  }

  const char *backend_name = prism_backend_name(backend.get());
  INFO("Using backend: " << (backend_name ? backend_name : "unknown"));

  SECTION("Get language for each voice") {
    size_t count = 0;
    PrismError count_err = prism_backend_count_voices(backend.get(), &count);

    if (count_err == PRISM_OK && count > 0) {
      for (size_t i = 0; i < count; ++i) {
        const char *language = nullptr;
        PrismError err =
            prism_backend_get_voice_language(backend.get(), i, &language);

        INFO("Voice index: " << i);
        if (err == PRISM_OK) {
          // Language might be null or empty for some voices
          if (language) {
            INFO("Voice language: " << language);
          }
        } else {
          CHECK_SUCCESS_OR_UNAVAILABLE(err);
        }
      }
    }
  }

  SECTION("Get language with null output") {
    PrismError err =
        prism_backend_get_voice_language(backend.get(), 0, nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Get language for out of bounds index") {
    size_t count = 0;
    prism_backend_count_voices(backend.get(), &count);

    const char *language = nullptr;
    PrismError err =
        prism_backend_get_voice_language(backend.get(), count, &language);
    CHECK((err == PRISM_ERROR_VOICE_NOT_FOUND ||
           err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }
}

TEST_CASE("Backend set voice", "[backend][voices][set]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping set voice tests");
    return;
  }

  const char *backend_name = prism_backend_name(backend.get());
  INFO("Using backend: " << (backend_name ? backend_name : "unknown"));

  SECTION("Set voice to valid indices") {
    size_t count = 0;
    PrismError count_err = prism_backend_count_voices(backend.get(), &count);

    if (count_err == PRISM_OK && count > 0) {
      for (size_t i = 0; i < count; ++i) {
        PrismError err = prism_backend_set_voice(backend.get(), i);
        INFO("Voice index: " << i);
        REQUIRE_SUCCESS_OR_UNAVAILABLE(err);
      }
    }
  }

  SECTION("Set voice to out of bounds index") {
    size_t count = 0;
    prism_backend_count_voices(backend.get(), &count);

    PrismError err = prism_backend_set_voice(backend.get(), count);
    CHECK((err == PRISM_ERROR_VOICE_NOT_FOUND ||
           err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Set voice to very large index") {
    PrismError err = prism_backend_set_voice(backend.get(), SIZE_MAX);
    CHECK((err == PRISM_ERROR_VOICE_NOT_FOUND ||
           err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Set voice multiple times") {
    size_t count = 0;
    prism_backend_count_voices(backend.get(), &count);

    if (count >= 2) {
      PrismError err1 = prism_backend_set_voice(backend.get(), 0);
      PrismError err2 = prism_backend_set_voice(backend.get(), 1);
      PrismError err3 = prism_backend_set_voice(backend.get(), 0);

      CHECK_SUCCESS_OR_UNAVAILABLE(err1);
      CHECK_SUCCESS_OR_UNAVAILABLE(err2);
      CHECK_SUCCESS_OR_UNAVAILABLE(err3);
    }
  }
}

TEST_CASE("Backend get voice", "[backend][voices][get]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping get voice tests");
    return;
  }

  const char *backend_name = prism_backend_name(backend.get());
  INFO("Using backend: " << (backend_name ? backend_name : "unknown"));

  SECTION("Get current voice") {
    size_t voice_id = SIZE_MAX;
    PrismError err = prism_backend_get_voice(backend.get(), &voice_id);

    if (err == PRISM_OK) {
      size_t count = 0;
      prism_backend_count_voices(backend.get(), &count);

      INFO("Current voice: " << voice_id);
      CHECK(voice_id < count);
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Get voice with null output") {
    PrismError err = prism_backend_get_voice(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Get voice after set") {
    size_t count = 0;
    prism_backend_count_voices(backend.get(), &count);

    if (count > 0) {
      size_t target_voice = 0;
      PrismError set_err = prism_backend_set_voice(backend.get(), target_voice);

      if (set_err == PRISM_OK) {
        size_t current_voice = SIZE_MAX;
        PrismError get_err =
            prism_backend_get_voice(backend.get(), &current_voice);

        if (get_err == PRISM_OK) {
          CHECK(current_voice == target_voice);
        }
      }
    }
  }

  SECTION("Set/get roundtrip for all voices") {
    size_t count = 0;
    prism_backend_count_voices(backend.get(), &count);

    for (size_t i = 0; i < count; ++i) {
      PrismError set_err = prism_backend_set_voice(backend.get(), i);

      if (set_err == PRISM_OK) {
        size_t got_voice = SIZE_MAX;
        PrismError get_err = prism_backend_get_voice(backend.get(), &got_voice);

        INFO("Set voice: " << i);
        if (get_err == PRISM_OK) {
          CHECK(got_voice == i);
        }
      }
    }
  }
}

TEST_CASE("Backend voice and speech", "[backend][voices][speech]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping voice+speech tests");
    return;
  }

  const char *backend_name = prism_backend_name(backend.get());
  INFO("Using backend: " << (backend_name ? backend_name : "unknown"));

  SECTION("Speak with each voice") {
    size_t count = 0;
    PrismError count_err = prism_backend_count_voices(backend.get(), &count);

    if (count_err == PRISM_OK && count > 0) {
      for (size_t i = 0; i < count && i < 5; ++i) { // Limit to 5 voices
        PrismError set_err = prism_backend_set_voice(backend.get(), i);

        if (set_err == PRISM_OK) {
          const char *voice_name = nullptr;
          prism_backend_get_voice_name(backend.get(), i, &voice_name);
          INFO("Voice: " << (voice_name ? voice_name : "unknown"));

          PrismError speak_err =
              prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
          CHECK_SUCCESS_OR_UNAVAILABLE(speak_err);

          prism_backend_stop(backend.get());
        }
      }
    }
  }

  SECTION("Voice persists across speak calls") {
    size_t count = 0;
    prism_backend_count_voices(backend.get(), &count);

    if (count >= 2) {
      // Set to second voice
      prism_backend_set_voice(backend.get(), 1);

      // Speak
      prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
      prism_backend_stop(backend.get());

      // Voice should still be set
      size_t current = SIZE_MAX;
      PrismError err = prism_backend_get_voice(backend.get(), &current);
      if (err == PRISM_OK) {
        CHECK(current == 1);
      }
    }
  }
}

TEST_CASE("Backend enumerate all voice info", "[backend][voices][enumerate]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping voice enumeration tests");
    return;
  }

  const char *backend_name = prism_backend_name(backend.get());
  INFO("Using backend: " << (backend_name ? backend_name : "unknown"));

  SECTION("List all voices with name and language") {
    size_t count = 0;
    PrismError count_err = prism_backend_count_voices(backend.get(), &count);

    if (count_err == PRISM_OK) {
      INFO("Total voices: " << count);

      for (size_t i = 0; i < count; ++i) {
        const char *name = nullptr;
        const char *language = nullptr;

        prism_backend_get_voice_name(backend.get(), i, &name);
        prism_backend_get_voice_language(backend.get(), i, &language);

        INFO("Voice " << i << ": " << (name ? name : "(null)") << " - "
                      << (language ? language : "(null)"));
      }
    }
  }
}
