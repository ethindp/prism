#include "test_helpers.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <prism.h>

using namespace prism_test;
using Catch::Approx;

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

TEST_CASE("Backend volume control", "[backend][controls][volume]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping volume tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Get initial volume") {
    float volume = -1.0f;
    PrismError err = prism_backend_get_volume(backend.get(), &volume);

    if (err == PRISM_OK) {
      CHECK(volume >= 0.0f);
      CHECK(volume <= 1.0f);
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Set and get volume") {
    const float test_volumes[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float test_vol : test_volumes) {
      PrismError set_err = prism_backend_set_volume(backend.get(), test_vol);

      if (set_err == PRISM_OK) {
        float got_vol = -1.0f;
        PrismError get_err = prism_backend_get_volume(backend.get(), &got_vol);

        if (get_err == PRISM_OK) {
          INFO("Set: " << test_vol << ", Got: " << got_vol);
          CHECK(got_vol == Approx(test_vol).margin(0.05f));
        }
      } else {
        CHECK_SUCCESS_OR_UNAVAILABLE(set_err);
      }
    }
  }

  SECTION("Set volume below minimum") {
    PrismError err = prism_backend_set_volume(backend.get(), -0.5f);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));

    // If it succeeded, volume should be clamped to minimum
    if (err == PRISM_OK) {
      float volume = -1.0f;
      prism_backend_get_volume(backend.get(), &volume);
      CHECK(volume >= 0.0f);
    }
  }

  SECTION("Set volume above maximum") {
    PrismError err = prism_backend_set_volume(backend.get(), 1.5f);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));

    // If it succeeded, volume should be clamped to maximum
    if (err == PRISM_OK) {
      float volume = -1.0f;
      prism_backend_get_volume(backend.get(), &volume);
      CHECK(volume <= 1.0f);
    }
  }

  SECTION("Get volume with null output") {
    PrismError err = prism_backend_get_volume(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Volume persists across operations") {
    PrismError set_err = prism_backend_set_volume(backend.get(), 0.5f);

    if (set_err == PRISM_OK) {
      // Do some operations
      prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
      prism_backend_stop(backend.get());

      float volume = -1.0f;
      PrismError get_err = prism_backend_get_volume(backend.get(), &volume);
      if (get_err == PRISM_OK) {
        CHECK(volume == Approx(0.5f).margin(0.05f));
      }
    }
  }

  SECTION("Set volume to exact boundaries") {
    PrismError err0 = prism_backend_set_volume(backend.get(), 0.0f);
    if (err0 == PRISM_OK) {
      float vol = -1.0f;
      prism_backend_get_volume(backend.get(), &vol);
      CHECK(vol == Approx(0.0f).margin(0.01f));
    }

    PrismError err1 = prism_backend_set_volume(backend.get(), 1.0f);
    if (err1 == PRISM_OK) {
      float vol = -1.0f;
      prism_backend_get_volume(backend.get(), &vol);
      CHECK(vol == Approx(1.0f).margin(0.01f));
    }
  }
}

TEST_CASE("Backend rate control", "[backend][controls][rate]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping rate tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Get initial rate") {
    float rate = -1.0f;
    PrismError err = prism_backend_get_rate(backend.get(), &rate);

    if (err == PRISM_OK) {
      CHECK(rate >= 0.0f);
      CHECK(rate <= 1.0f);
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Set and get rate") {
    const float test_rates[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float test_rate : test_rates) {
      PrismError set_err = prism_backend_set_rate(backend.get(), test_rate);

      if (set_err == PRISM_OK) {
        float got_rate = -1.0f;
        PrismError get_err = prism_backend_get_rate(backend.get(), &got_rate);

        if (get_err == PRISM_OK) {
          INFO("Set: " << test_rate << ", Got: " << got_rate);
          CHECK(got_rate == Approx(test_rate).margin(0.1f));
        }
      } else {
        CHECK_SUCCESS_OR_UNAVAILABLE(set_err);
      }
    }
  }

  SECTION("Set rate at minimum") {
    PrismError err = prism_backend_set_rate(backend.get(), 0.0f);
    CHECK_SUCCESS_OR_UNAVAILABLE(err);

    if (err == PRISM_OK) {
      float rate = -1.0f;
      prism_backend_get_rate(backend.get(), &rate);
      CHECK(rate >= 0.0f);
    }
  }

  SECTION("Set rate at maximum") {
    PrismError err = prism_backend_set_rate(backend.get(), 1.0f);
    CHECK_SUCCESS_OR_UNAVAILABLE(err);

    if (err == PRISM_OK) {
      float rate = -1.0f;
      prism_backend_get_rate(backend.get(), &rate);
      CHECK(rate <= 1.0f);
    }
  }

  SECTION("Set rate below minimum") {
    PrismError err = prism_backend_set_rate(backend.get(), -1.0f);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Set rate above maximum") {
    PrismError err = prism_backend_set_rate(backend.get(), 1.5f);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Get rate with null output") {
    PrismError err = prism_backend_get_rate(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Rate persists across operations") {
    PrismError set_err = prism_backend_set_rate(backend.get(), 0.75f);

    if (set_err == PRISM_OK) {
      prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
      prism_backend_stop(backend.get());

      float rate = -1.0f;
      PrismError get_err = prism_backend_get_rate(backend.get(), &rate);
      if (get_err == PRISM_OK) {
        CHECK(rate == Approx(0.75f).margin(0.1f));
      }
    }
  }
}

TEST_CASE("Backend pitch control", "[backend][controls][pitch]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping pitch tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Get initial pitch") {
    float pitch = -1.0f;
    PrismError err = prism_backend_get_pitch(backend.get(), &pitch);

    if (err == PRISM_OK) {
      CHECK(pitch >= 0.0f);
      CHECK(pitch <= 1.0f);
    } else {
      CHECK_SUCCESS_OR_UNAVAILABLE(err);
    }
  }

  SECTION("Set and get pitch") {
    const float test_pitches[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float test_pitch : test_pitches) {
      PrismError set_err = prism_backend_set_pitch(backend.get(), test_pitch);

      if (set_err == PRISM_OK) {
        float got_pitch = -1.0f;
        PrismError get_err = prism_backend_get_pitch(backend.get(), &got_pitch);

        if (get_err == PRISM_OK) {
          INFO("Set: " << test_pitch << ", Got: " << got_pitch);
          CHECK(got_pitch == Approx(test_pitch).margin(0.1f));
        }
      } else {
        CHECK_SUCCESS_OR_UNAVAILABLE(set_err);
      }
    }
  }

  SECTION("Set pitch at minimum") {
    PrismError err = prism_backend_set_pitch(backend.get(), 0.0f);
    CHECK_SUCCESS_OR_UNAVAILABLE(err);

    if (err == PRISM_OK) {
      float pitch = -1.0f;
      prism_backend_get_pitch(backend.get(), &pitch);
      CHECK(pitch >= 0.0f);
    }
  }

  SECTION("Set pitch at maximum") {
    PrismError err = prism_backend_set_pitch(backend.get(), 1.0f);
    CHECK_SUCCESS_OR_UNAVAILABLE(err);

    if (err == PRISM_OK) {
      float pitch = -1.0f;
      prism_backend_get_pitch(backend.get(), &pitch);
      CHECK(pitch <= 1.0f);
    }
  }

  SECTION("Set pitch below minimum") {
    PrismError err = prism_backend_set_pitch(backend.get(), -1.0f);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Set pitch above maximum") {
    PrismError err = prism_backend_set_pitch(backend.get(), 1.5f);
    CHECK((err == PRISM_OK || err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           err == PRISM_ERROR_INVALID_PARAM || is_unavailable_error(err)));
  }

  SECTION("Get pitch with null output") {
    PrismError err = prism_backend_get_pitch(backend.get(), nullptr);
    CHECK(err == PRISM_ERROR_INVALID_PARAM);
  }

  SECTION("Pitch persists across operations") {
    PrismError set_err = prism_backend_set_pitch(backend.get(), 0.75f);

    if (set_err == PRISM_OK) {
      prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
      prism_backend_stop(backend.get());

      float pitch = -1.0f;
      PrismError get_err = prism_backend_get_pitch(backend.get(), &pitch);
      if (get_err == PRISM_OK) {
        CHECK(pitch == Approx(0.75f).margin(0.1f));
      }
    }
  }
}

TEST_CASE("Backend combined controls", "[backend][controls][combined]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping combined control tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("Set all controls simultaneously") {
    PrismError vol_err = prism_backend_set_volume(backend.get(), 0.8f);
    PrismError rate_err = prism_backend_set_rate(backend.get(), 0.6f);
    PrismError pitch_err = prism_backend_set_pitch(backend.get(), 0.9f);

    // Verify all were set
    if (vol_err == PRISM_OK && rate_err == PRISM_OK && pitch_err == PRISM_OK) {
      float volume = -1.0f, rate = -1.0f, pitch = -1.0f;

      prism_backend_get_volume(backend.get(), &volume);
      prism_backend_get_rate(backend.get(), &rate);
      prism_backend_get_pitch(backend.get(), &pitch);

      CHECK(volume == Approx(0.8f).margin(0.05f));
      CHECK(rate == Approx(0.6f).margin(0.1f));
      CHECK(pitch == Approx(0.9f).margin(0.1f));
    }
  }

  SECTION("Controls are independent") {
    // Set initial values
    prism_backend_set_volume(backend.get(), 0.5f);
    prism_backend_set_rate(backend.get(), 0.5f);
    prism_backend_set_pitch(backend.get(), 0.5f);

    // Change only volume
    prism_backend_set_volume(backend.get(), 1.0f);

    float rate = -1.0f, pitch = -1.0f;
    PrismError rate_err = prism_backend_get_rate(backend.get(), &rate);
    PrismError pitch_err = prism_backend_get_pitch(backend.get(), &pitch);

    if (rate_err == PRISM_OK && pitch_err == PRISM_OK) {
      CHECK(rate == Approx(0.5f).margin(0.1f));
      CHECK(pitch == Approx(0.5f).margin(0.1f));
    }
  }

  SECTION("Speak with custom settings") {
    prism_backend_set_volume(backend.get(), 0.7f);
    prism_backend_set_rate(backend.get(), 0.75f);
    prism_backend_set_pitch(backend.get(), 0.8f);

    PrismError err =
        prism_backend_speak(backend.get(), strings::HELLO_WORLD, true);
    CHECK_SUCCESS_OR_UNAVAILABLE(err);

    prism_backend_stop(backend.get());
  }

  SECTION("Reset to defaults") {
    // Set non-default values
    prism_backend_set_volume(backend.get(), 0.3f);
    prism_backend_set_rate(backend.get(), 0.75f);
    prism_backend_set_pitch(backend.get(), 0.25f);

    // Reset to defaults
    prism_backend_set_volume(backend.get(), DEFAULT_VOLUME);
    prism_backend_set_rate(backend.get(), DEFAULT_RATE);
    prism_backend_set_pitch(backend.get(), DEFAULT_PITCH);

    float volume = -1.0f, rate = -1.0f, pitch = -1.0f;

    PrismError vol_err = prism_backend_get_volume(backend.get(), &volume);
    PrismError rate_err = prism_backend_get_rate(backend.get(), &rate);
    PrismError pitch_err = prism_backend_get_pitch(backend.get(), &pitch);

    if (vol_err == PRISM_OK)
      CHECK(volume == Approx(DEFAULT_VOLUME).margin(0.05f));
    if (rate_err == PRISM_OK)
      CHECK(rate == Approx(DEFAULT_RATE).margin(0.1f));
    if (pitch_err == PRISM_OK)
      CHECK(pitch == Approx(DEFAULT_PITCH).margin(0.1f));
  }
}

TEST_CASE("Backend control edge cases", "[backend][controls][edge]") {
  ContextPtr ctx = make_context();
  REQUIRE(ctx.get() != nullptr);

  BackendPtr backend(get_initialized_backend(ctx.get()));

  if (!backend) {
    WARN("No initialized backend available, skipping edge case tests");
    return;
  }

  const char *name = prism_backend_name(backend.get());
  INFO("Using backend: " << (name ? name : "unknown"));

  SECTION("NaN values") {
    float nan_value = std::nan("");

    PrismError vol_err = prism_backend_set_volume(backend.get(), nan_value);
    CHECK((vol_err == PRISM_OK || vol_err == PRISM_ERROR_INVALID_PARAM ||
           vol_err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           is_unavailable_error(vol_err)));

    PrismError rate_err = prism_backend_set_rate(backend.get(), nan_value);
    CHECK((rate_err == PRISM_OK || rate_err == PRISM_ERROR_INVALID_PARAM ||
           rate_err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           is_unavailable_error(rate_err)));

    PrismError pitch_err = prism_backend_set_pitch(backend.get(), nan_value);
    CHECK((pitch_err == PRISM_OK || pitch_err == PRISM_ERROR_INVALID_PARAM ||
           pitch_err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           is_unavailable_error(pitch_err)));
  }

  SECTION("Infinity values") {
    float inf_value = std::numeric_limits<float>::infinity();

    PrismError vol_err = prism_backend_set_volume(backend.get(), inf_value);
    CHECK((vol_err == PRISM_OK || vol_err == PRISM_ERROR_INVALID_PARAM ||
           vol_err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           is_unavailable_error(vol_err)));

    PrismError rate_err = prism_backend_set_rate(backend.get(), inf_value);
    CHECK((rate_err == PRISM_OK || rate_err == PRISM_ERROR_INVALID_PARAM ||
           rate_err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           is_unavailable_error(rate_err)));

    PrismError pitch_err = prism_backend_set_pitch(backend.get(), inf_value);
    CHECK((pitch_err == PRISM_OK || pitch_err == PRISM_ERROR_INVALID_PARAM ||
           pitch_err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           is_unavailable_error(pitch_err)));
  }

  SECTION("Negative infinity values") {
    float neg_inf = -std::numeric_limits<float>::infinity();

    PrismError vol_err = prism_backend_set_volume(backend.get(), neg_inf);
    CHECK((vol_err == PRISM_OK || vol_err == PRISM_ERROR_INVALID_PARAM ||
           vol_err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           is_unavailable_error(vol_err)));

    PrismError rate_err = prism_backend_set_rate(backend.get(), neg_inf);
    CHECK((rate_err == PRISM_OK || rate_err == PRISM_ERROR_INVALID_PARAM ||
           rate_err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           is_unavailable_error(rate_err)));

    PrismError pitch_err = prism_backend_set_pitch(backend.get(), neg_inf);
    CHECK((pitch_err == PRISM_OK || pitch_err == PRISM_ERROR_INVALID_PARAM ||
           pitch_err == PRISM_ERROR_RANGE_OUT_OF_BOUNDS ||
           is_unavailable_error(pitch_err)));
  }

  SECTION("Very small positive values") {
    float tiny = std::numeric_limits<float>::min();

    PrismError vol_err = prism_backend_set_volume(backend.get(), tiny);
    CHECK_SUCCESS_OR_UNAVAILABLE(vol_err);

    PrismError rate_err = prism_backend_set_rate(backend.get(), tiny);
    CHECK_SUCCESS_OR_UNAVAILABLE(rate_err);

    PrismError pitch_err = prism_backend_set_pitch(backend.get(), tiny);
    CHECK_SUCCESS_OR_UNAVAILABLE(pitch_err);
  }

  SECTION("Denormalized values") {
    float denorm = std::numeric_limits<float>::denorm_min();

    PrismError vol_err = prism_backend_set_volume(backend.get(), denorm);
    CHECK((vol_err == PRISM_OK || vol_err == PRISM_ERROR_INVALID_PARAM ||
           is_unavailable_error(vol_err)));
  }

  SECTION("Rapid control changes") {
    for (int i = 0; i < 100; ++i) {
      float val = static_cast<float>(i % 100) / 100.0f;
      prism_backend_set_volume(backend.get(), val);
    }

    // Should not crash and final value should be valid
    float volume = -1.0f;
    PrismError err = prism_backend_get_volume(backend.get(), &volume);
    if (err == PRISM_OK) {
      CHECK(volume >= 0.0f);
      CHECK(volume <= 1.0f);
    }
  }
}
