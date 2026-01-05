#include "test_helpers.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <prism.h>
#include <set>
#include <thread>

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

TEST_CASE("Backend get channels", "[backend][audio][channels]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping channel tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Get channels returns valid value") {
    size_t channels = 0;
    PrismError err = prism_backend_get_channels(backend.get(), &channels);

    if (err == PRISM_OK) {
      INFO("Channels: " << channels);
      CHECK(channels >= 1);
      CHECK(channels <= 8); // Reasonable max for audio
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Get channels with null output") {
    PrismError err = prism_backend_get_channels(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Channels is consistent") {
    size_t ch1 = 0, ch2 = 0;

    PrismError err1 = prism_backend_get_channels(backend.get(), &ch1);
    PrismError err2 = prism_backend_get_channels(backend.get(), &ch2);

    if (err1 == PRISM_OK && err2 == PRISM_OK) {
      CHECK(ch1 == ch2);
    }
  }

  SECTION("Common channel counts") {
    size_t channels = 0;
    PrismError err = prism_backend_get_channels(backend.get(), &channels);

    if (err == PRISM_OK) {
      // Most TTS systems use mono or stereo
      std::set<size_t> common_counts = {1, 2};
      INFO("Channels: " << channels);
      // Just informational - don't fail on unusual values
    }
  }
}

TEST_CASE("Backend get sample rate", "[backend][audio][sample_rate]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping sample rate tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Get sample rate returns valid value") {
    size_t sample_rate = 0;
    PrismError err = prism_backend_get_sample_rate(backend.get(), &sample_rate);

    if (err == PRISM_OK) {
      INFO("Sample rate: " << sample_rate);
      CHECK(sample_rate >= 8000);   // Minimum reasonable rate
      CHECK(sample_rate <= 192000); // Maximum reasonable rate
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Get sample rate with null output") {
    PrismError err = prism_backend_get_sample_rate(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Sample rate is consistent") {
    size_t sr1 = 0, sr2 = 0;

    PrismError err1 = prism_backend_get_sample_rate(backend.get(), &sr1);
    PrismError err2 = prism_backend_get_sample_rate(backend.get(), &sr2);

    if (err1 == PRISM_OK && err2 == PRISM_OK) {
      CHECK(sr1 == sr2);
    }
  }

  SECTION("Common sample rates") {
    size_t sample_rate = 0;
    PrismError err = prism_backend_get_sample_rate(backend.get(), &sample_rate);

    if (err == PRISM_OK) {
      std::set<size_t> common_rates = {8000,  11025, 16000, 22050, 24000,
                                       32000, 44100, 48000, 96000};
      INFO("Sample rate: " << sample_rate);
      // Just informational
    }
  }
}

TEST_CASE("Backend get bit depth", "[backend][audio][bit_depth]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping bit depth tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Get bit depth returns valid value") {
    size_t bit_depth = 0;
    PrismError err = prism_backend_get_bit_depth(backend.get(), &bit_depth);

    if (err == PRISM_OK) {
      INFO("Bit depth: " << bit_depth);
      // Common bit depths: 8, 16, 24, 32
      CHECK(bit_depth >= 8);
      CHECK(bit_depth <= 64);
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Get bit depth with null output") {
    PrismError err = prism_backend_get_bit_depth(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Bit depth is consistent") {
    size_t bd1 = 0, bd2 = 0;

    PrismError err1 = prism_backend_get_bit_depth(backend.get(), &bd1);
    PrismError err2 = prism_backend_get_bit_depth(backend.get(), &bd2);

    if (err1 == PRISM_OK && err2 == PRISM_OK) {
      CHECK(bd1 == bd2);
    }
  }

  SECTION("Common bit depths") {
    size_t bit_depth = 0;
    PrismError err = prism_backend_get_bit_depth(backend.get(), &bit_depth);

    if (err == PRISM_OK) {
      std::set<size_t> common_depths = {8, 16, 24, 32};
      INFO("Bit depth: " << bit_depth);
      // Just informational
    }
  }
}

TEST_CASE("Backend audio format combined", "[backend][audio][format]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping audio format tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Get complete audio format") {
    size_t channels = 0, sample_rate = 0, bit_depth = 0;

    PrismError ch_err = prism_backend_get_channels(backend.get(), &channels);
    PrismError sr_err =
        prism_backend_get_sample_rate(backend.get(), &sample_rate);
    PrismError bd_err = prism_backend_get_bit_depth(backend.get(), &bit_depth);

    INFO("Format: " << channels << " channels, " << sample_rate << " Hz, "
                    << bit_depth << " bits");

    if (ch_err == PRISM_OK && sr_err == PRISM_OK && bd_err == PRISM_OK) {
      // Calculate bytes per sample and per second
      size_t bytes_per_sample = bit_depth / 8;
      size_t bytes_per_second = sample_rate * channels * bytes_per_sample;

      INFO("Bytes per sample: " << bytes_per_sample);
      INFO("Bytes per second: " << bytes_per_second);

      CHECK(bytes_per_sample >= 1);
      CHECK(bytes_per_second > 0);
    }
  }

  SECTION("Audio format is valid for speech") {
    size_t channels = 0, sample_rate = 0, bit_depth = 0;

    prism_backend_get_channels(backend.get(), &channels);
    prism_backend_get_sample_rate(backend.get(), &sample_rate);
    prism_backend_get_bit_depth(backend.get(), &bit_depth);

    // For speech, typical formats:
    // - 1 channel (mono) is common
    // - 16000-48000 Hz is typical for speech
    // - 16 or 32 bit depth

    INFO("Channels: " << channels);
    INFO("Sample rate: " << sample_rate);
    INFO("Bit depth: " << bit_depth);
  }

  SECTION("Format consistency across voices") {
    size_t voice_count = 0;
    prism_backend_count_voices(backend.get(), &voice_count);

    if (voice_count >= 2) {
      // Get format with first voice
      prism_backend_set_voice(backend.get(), 0);

      size_t ch1 = 0, sr1 = 0, bd1 = 0;
      prism_backend_get_channels(backend.get(), &ch1);
      prism_backend_get_sample_rate(backend.get(), &sr1);
      prism_backend_get_bit_depth(backend.get(), &bd1);

      // Get format with second voice
      prism_backend_set_voice(backend.get(), 1);

      size_t ch2 = 0, sr2 = 0, bd2 = 0;
      prism_backend_get_channels(backend.get(), &ch2);
      prism_backend_get_sample_rate(backend.get(), &sr2);
      prism_backend_get_bit_depth(backend.get(), &bd2);

      INFO("Voice 0: " << ch1 << "ch, " << sr1 << "Hz, " << bd1 << "bit");
      INFO("Voice 1: " << ch2 << "ch, " << sr2 << "Hz, " << bd2 << "bit");

      // Format may or may not change between voices
    }
  }
}

TEST_CASE("Backend audio callback format", "[backend][audio][callback]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping callback format tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Callback receives correct format info") {
    AudioCallbackData data;

    PrismError err = prism_backend_speak_to_memory(
        backend.get(), strings::HELLO_WORLD, test_audio_callback, &data);

    if (err == PRISM_OK) {
      // Wait for some audio
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      if (data.callback_count > 0) {
        INFO("Callback channels: " << data.channels);
        INFO("Callback sample rate: " << data.sample_rate);
        INFO("Samples received: " << data.samples.size());

        // Verify callback format matches getter format
        size_t expected_channels = 0;
        size_t expected_rate = 0;

        prism_backend_get_channels(backend.get(), &expected_channels);
        prism_backend_get_sample_rate(backend.get(), &expected_rate);

        if (expected_channels > 0) {
          CHECK(data.channels == expected_channels);
        }
        if (expected_rate > 0) {
          CHECK(data.sample_rate == expected_rate);
        }
      }
    }
  }
}

TEST_CASE("Backend audio format for each backend",
          "[backend][audio][all_backends]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  size_t count = prism_registry_count(ctx.get());

  SECTION("Query audio format from all backends") {
    for (size_t i = 0; i < count; ++i) {
      PrismBackendId id = prism_registry_id_at(ctx.get(), i);
      BackendPtr backend = make_backend(ctx.get(), id);

      if (!backend)
        continue;

      const char *name = prism_backend_name(backend.get());
      INFO("Backend: " << (name ? name : "unknown"));

      PrismError init_err = prism_backend_initialize(backend.get());
      if (init_err != PRISM_OK)
        continue;

      size_t channels = 0, sample_rate = 0, bit_depth = 0;

      PrismError ch_err = prism_backend_get_channels(backend.get(), &channels);
      PrismError sr_err =
          prism_backend_get_sample_rate(backend.get(), &sample_rate);
      PrismError bd_err =
          prism_backend_get_bit_depth(backend.get(), &bit_depth);

      INFO("  Channels: " << channels << " (err: " << prism_error_string(ch_err)
                          << ")");
      INFO("  Sample rate: " << sample_rate
                             << " (err: " << prism_error_string(sr_err) << ")");
      INFO("  Bit depth: " << bit_depth
                           << " (err: " << prism_error_string(bd_err) << ")");
    }
  }
}

TEST_CASE("Backend audio format edge cases", "[backend][audio][edge]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping edge case tests");
    return;
  }

  SECTION("Format queries before initialization") {
    // Create but don't initialize
    BackendPtr uninit_backend = make_best_backend(ctx.get());

    if (uninit_backend) {
      size_t value = 0;

      PrismError ch_err =
          prism_backend_get_channels(uninit_backend.get(), &value);
      PrismError sr_err =
          prism_backend_get_sample_rate(uninit_backend.get(), &value);
      PrismError bd_err =
          prism_backend_get_bit_depth(uninit_backend.get(), &value);

      // Might succeed or fail depending on implementation
      (void)ch_err;
      (void)sr_err;
      (void)bd_err;
    }
  }

  SECTION("Format queries during speech") {
    PrismError speak_err =
        prism_backend_speak(backend.get(), strings::LONG_TEXT, false);

    if (speak_err == PRISM_OK) {
      size_t channels = 0, sample_rate = 0, bit_depth = 0;

      // Should work during speech
      prism_backend_get_channels(backend.get(), &channels);
      prism_backend_get_sample_rate(backend.get(), &sample_rate);
      prism_backend_get_bit_depth(backend.get(), &bit_depth);

      prism_backend_stop(backend.get());
    }
  }

  SECTION("Rapid format queries") {
    for (int i = 0; i < 100; ++i) {
      size_t channels = 0, sample_rate = 0, bit_depth = 0;

      prism_backend_get_channels(backend.get(), &channels);
      prism_backend_get_sample_rate(backend.get(), &sample_rate);
      prism_backend_get_bit_depth(backend.get(), &bit_depth);
    }
  }
}
