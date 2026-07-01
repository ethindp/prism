// SPDX-License-Identifier: MPL-2.0
#include "backend_registry.h"
#include "prism.h"
#include <algorithm>
#include <bitset>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
BackendError from_prism_error(PrismError error) noexcept {
  const auto value = std::to_underlying(error);
  if (std::cmp_greater_equal(value, std::to_underlying(PRISM_ERROR_COUNT)))
    return BackendError::Unknown;
  return static_cast<BackendError>(value);
}

BackendResult<> to_result(PrismError error) {
  if (error == PRISM_OK)
    return {};
  return std::unexpected(from_prism_error(error));
}

bool vtable_consistent(std::uint64_t features,
                       const PrismBackendVTable &vtable) noexcept {
  using namespace BackendFeature;
  struct FeatureSlot {
    std::uint64_t feature;
    bool present;
  };
  const auto slots = std::to_array<FeatureSlot>({
      {.feature = SUPPORTS_SPEAK, .present = vtable.speak != nullptr},
      {.feature = SUPPORTS_SPEAK_TO_MEMORY,
       .present = vtable.speak_to_memory != nullptr},
      {.feature = SUPPORTS_BRAILLE, .present = vtable.braille != nullptr},
      {.feature = SUPPORTS_OUTPUT, .present = vtable.output != nullptr},
      {.feature = SUPPORTS_IS_SPEAKING,
       .present = vtable.is_speaking != nullptr},
      {.feature = SUPPORTS_STOP, .present = vtable.stop != nullptr},
      {.feature = SUPPORTS_PAUSE, .present = vtable.pause != nullptr},
      {.feature = SUPPORTS_RESUME, .present = vtable.resume != nullptr},
      {.feature = SUPPORTS_SET_VOLUME, .present = vtable.set_volume != nullptr},
      {.feature = SUPPORTS_GET_VOLUME, .present = vtable.get_volume != nullptr},
      {.feature = SUPPORTS_SET_RATE, .present = vtable.set_rate != nullptr},
      {.feature = SUPPORTS_GET_RATE, .present = vtable.get_rate != nullptr},
      {.feature = SUPPORTS_SET_PITCH, .present = vtable.set_pitch != nullptr},
      {.feature = SUPPORTS_GET_PITCH, .present = vtable.get_pitch != nullptr},
      {.feature = SUPPORTS_REFRESH_VOICES,
       .present = vtable.refresh_voices != nullptr},
      {.feature = SUPPORTS_COUNT_VOICES,
       .present = vtable.count_voices != nullptr},
      {.feature = SUPPORTS_GET_VOICE_NAME,
       .present = vtable.get_voice_name != nullptr},
      {.feature = SUPPORTS_GET_VOICE_LANGUAGE,
       .present = vtable.get_voice_language != nullptr},
      {.feature = SUPPORTS_GET_VOICE, .present = vtable.get_voice != nullptr},
      {.feature = SUPPORTS_SET_VOICE, .present = vtable.set_voice != nullptr},
      {.feature = SUPPORTS_GET_CHANNELS,
       .present = vtable.get_channels != nullptr},
      {.feature = SUPPORTS_GET_SAMPLE_RATE,
       .present = vtable.get_sample_rate != nullptr},
      {.feature = SUPPORTS_GET_BIT_DEPTH,
       .present = vtable.get_bit_depth != nullptr},
  });
  return std::ranges::all_of(slots, [features](const FeatureSlot &slot) {
    return ((features & slot.feature) != 0) == slot.present;
  });
}

struct CustomRegistration {
  PrismBackendVTable vtable;
  void *userdata;
  void (*userdata_free)(void *);
  std::bitset<64> features;
  std::string name;
  CustomRegistration(const PrismBackendVTable &vtable, void *userdata,
                     void (*userdata_free)(void *), std::uint64_t features,
                     std::string name)
      : vtable(vtable), userdata(userdata), userdata_free(userdata_free),
        features(features), name(std::move(name)) {}
  ~CustomRegistration() {
    if (userdata_free != nullptr)
      userdata_free(userdata);
  }
  CustomRegistration(const CustomRegistration &) = delete;
  CustomRegistration &operator=(const CustomRegistration &) = delete;
};

struct MemoryBridge {
  const TextToSpeechBackend::AudioCallback *callback;
  void *userdata;
  std::vector<float> scratch;
};

void PRISM_CALL memory_trampoline(void *userdata, const float *samples,
                                  std::size_t sample_count,
                                  std::size_t channels,
                                  std::size_t sample_rate) {
  auto *bridge = static_cast<MemoryBridge *>(userdata);
  bridge->scratch.assign(samples, samples + sample_count);
  for (float &sample : bridge->scratch)
    sample = std::isfinite(sample) ? std::clamp(sample, -1.0F, 1.0F) : 0.0F;
  (*bridge->callback)(bridge->userdata, bridge->scratch.data(), sample_count,
                      channels, sample_rate);
}
} // namespace

class CustomBackend final : public TextToSpeechBackend {
private:
  std::shared_ptr<CustomRegistration> registration;
  void *instance;
  bool initialized = false;
  bool paused = false;

  template <typename Slot> BackendResult<> check(Slot slot) const {
    if (!initialized)
      return std::unexpected(BackendError::NotInitialized);
    if (slot == nullptr)
      return std::unexpected(BackendError::NotImplemented);
    return {};
  }

  template <typename Slot>
  BackendResult<> set_normalized(Slot slot, float value) {
    if (!std::isfinite(value) || value < 0.0F || value > 1.0F)
      return std::unexpected(BackendError::RangeOutOfBounds);
    if (const auto ready = check(slot); !ready)
      return std::unexpected(ready.error());
    return to_result(slot(instance, value));
  }

  template <typename Slot> BackendResult<float> get_normalized(Slot slot) {
    if (const auto ready = check(slot); !ready)
      return std::unexpected(ready.error());
    float value = 0.0F;
    if (const auto error = slot(instance, &value); error != PRISM_OK)
      return std::unexpected(from_prism_error(error));
    if (!std::isfinite(value))
      return std::unexpected(BackendError::BackendEnteredUndefinedState);
    return std::clamp(value, 0.0F, 1.0F);
  }

  template <typename Slot> BackendResult<std::size_t> get_size(Slot slot) {
    if (const auto ready = check(slot); !ready)
      return std::unexpected(ready.error());
    std::size_t value = 0;
    if (const auto error = slot(instance, &value); error != PRISM_OK)
      return std::unexpected(from_prism_error(error));
    return value;
  }

  template <typename Slot>
  BackendResult<std::string> get_string(Slot slot, std::size_t id) {
    if (const auto ready = check(slot); !ready)
      return std::unexpected(ready.error());
    const char *value = nullptr;
    if (const auto error = slot(instance, id, &value); error != PRISM_OK)
      return std::unexpected(from_prism_error(error));
    if (value == nullptr)
      return std::unexpected(BackendError::InternalBackendError);
    return std::string{value};
  }

public:
  CustomBackend(std::shared_ptr<CustomRegistration> registration,
                void *instance)
      : registration(std::move(registration)), instance(instance) {}

  ~CustomBackend() override {
    if (registration->vtable.create != nullptr &&
        registration->vtable.destroy != nullptr)
      registration->vtable.destroy(instance);
  }

  [[nodiscard]] std::string_view get_name() const override {
    return registration->name;
  }

  [[nodiscard]] std::bitset<64> get_features() const override {
    using namespace BackendFeature;
    auto bits = std::bitset<64>{registration->features};
    if (registration->vtable.is_supported != nullptr) {
      if (registration->vtable.is_supported(instance))
        bits |= IS_SUPPORTED_AT_RUNTIME;
      else
        bits &= ~IS_SUPPORTED_AT_RUNTIME;
    }
    return bits;
  }

  BackendResult<> initialize() override {
    if (initialized)
      return std::unexpected(BackendError::AlreadyInitialized);
    if (registration->vtable.initialize == nullptr) {
      initialized = true;
      return {};
    }
    const auto result = to_result(registration->vtable.initialize(instance));
    if (result)
      initialized = true;
    return result;
  }

  BackendResult<> speak(std::string_view text, bool interrupt) override {
    if (const auto ready = check(registration->vtable.speak); !ready)
      return std::unexpected(ready.error());
    const std::string owned{text};
    return to_result(
        registration->vtable.speak(instance, owned.c_str(), interrupt));
  }

  BackendResult<> speak_to_memory(std::string_view text, AudioCallback callback,
                                  void *userdata) override {
    if (const auto ready = check(registration->vtable.speak_to_memory); !ready)
      return std::unexpected(ready.error());
    const std::string owned{text};
    MemoryBridge bridge{
        .callback = &callback, .userdata = userdata, .scratch = {}};
    return to_result(registration->vtable.speak_to_memory(
        instance, owned.c_str(), &memory_trampoline, &bridge));
  }

  BackendResult<> braille(std::string_view text) override {
    if (const auto ready = check(registration->vtable.braille); !ready)
      return std::unexpected(ready.error());
    const std::string owned{text};
    return to_result(registration->vtable.braille(instance, owned.c_str()));
  }

  BackendResult<> output(std::string_view text, bool interrupt) override {
    if (const auto ready = check(registration->vtable.output); !ready)
      return std::unexpected(ready.error());
    const std::string owned{text};
    return to_result(
        registration->vtable.output(instance, owned.c_str(), interrupt));
  }

  BackendResult<bool> is_speaking() override {
    if (const auto ready = check(registration->vtable.is_speaking); !ready)
      return std::unexpected(ready.error());
    bool speaking = false;
    if (const auto error =
            registration->vtable.is_speaking(instance, &speaking);
        error != PRISM_OK)
      return std::unexpected(from_prism_error(error));
    return speaking;
  }

  BackendResult<> stop() override {
    if (const auto ready = check(registration->vtable.stop); !ready)
      return std::unexpected(ready.error());
    const auto result = to_result(registration->vtable.stop(instance));
    if (result)
      paused = false;
    return result;
  }

  BackendResult<> pause() override {
    if (const auto ready = check(registration->vtable.pause); !ready)
      return std::unexpected(ready.error());
    if (paused)
      return std::unexpected(BackendError::AlreadyPaused);
    const auto result = to_result(registration->vtable.pause(instance));
    if (result)
      paused = true;
    return result;
  }

  BackendResult<> resume() override {
    if (const auto ready = check(registration->vtable.resume); !ready)
      return std::unexpected(ready.error());
    if (!paused)
      return std::unexpected(BackendError::NotPaused);
    const auto result = to_result(registration->vtable.resume(instance));
    if (result)
      paused = false;
    return result;
  }

  BackendResult<> set_volume(float volume) override {
    return set_normalized(registration->vtable.set_volume, volume);
  }

  BackendResult<float> get_volume() override {
    return get_normalized(registration->vtable.get_volume);
  }

  BackendResult<> set_rate(float rate) override {
    return set_normalized(registration->vtable.set_rate, rate);
  }

  BackendResult<float> get_rate() override {
    return get_normalized(registration->vtable.get_rate);
  }

  BackendResult<> set_pitch(float pitch) override {
    return set_normalized(registration->vtable.set_pitch, pitch);
  }

  BackendResult<float> get_pitch() override {
    return get_normalized(registration->vtable.get_pitch);
  }

  BackendResult<> refresh_voices() override {
    if (const auto ready = check(registration->vtable.refresh_voices); !ready)
      return std::unexpected(ready.error());
    return to_result(registration->vtable.refresh_voices(instance));
  }

  BackendResult<std::size_t> count_voices() override {
    return get_size(registration->vtable.count_voices);
  }

  BackendResult<std::string> get_voice_name(std::size_t id) override {
    return get_string(registration->vtable.get_voice_name, id);
  }

  BackendResult<std::string> get_voice_language(std::size_t id) override {
    return get_string(registration->vtable.get_voice_language, id);
  }

  BackendResult<> set_voice(std::size_t id) override {
    if (const auto ready = check(registration->vtable.set_voice); !ready)
      return std::unexpected(ready.error());
    return to_result(registration->vtable.set_voice(instance, id));
  }

  BackendResult<std::size_t> get_voice() override {
    return get_size(registration->vtable.get_voice);
  }

  BackendResult<std::size_t> get_channels() override {
    return get_size(registration->vtable.get_channels);
  }

  BackendResult<std::size_t> get_sample_rate() override {
    return get_size(registration->vtable.get_sample_rate);
  }

  BackendResult<std::size_t> get_bit_depth() override {
    return get_size(registration->vtable.get_bit_depth);
  }
};

BackendFactory make_custom_factory(const PrismBackendVTable *vtable,
                                   void *userdata,
                                   void (*userdata_free)(void *),
                                   std::uint64_t features, std::string name) {
  if (vtable == nullptr || vtable->size == 0)
    return {};
  PrismBackendVTable normalized{};
  std::memcpy(&normalized, vtable,
              std::min(vtable->size, sizeof(PrismBackendVTable)));
  normalized.size = sizeof(PrismBackendVTable);
  if (!vtable_consistent(features, normalized))
    return {};
  auto registration = std::make_shared<CustomRegistration>(
      normalized, userdata, userdata_free, features, std::move(name));
  return [registration]() -> std::shared_ptr<TextToSpeechBackend> {
    void *instance = registration->vtable.create != nullptr
                         ? registration->vtable.create(registration->userdata)
                         : registration->userdata;
    if (registration->vtable.create != nullptr && instance == nullptr)
      return nullptr;
    return std::make_shared<CustomBackend>(registration, instance);
  };
}
