// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#ifdef __ANDROID__
#include <jni.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

enum class BackendError : std::uint8_t {
  Ok = 0,
  NotInitialized,
  InvalidParam,
  NotImplemented,
  NoVoices,
  VoiceNotFound,
  SpeakFailure,
  MemoryFailure,
  RangeOutOfBounds,
  InternalBackendError,
  NotSpeaking,
  NotPaused,
  AlreadyPaused,
  InvalidUtf8,
  InvalidOperation,
  AlreadyInitialized,
  BackendNotAvailable,
  Unknown,
  InvalidAudioFormat
};

template <typename T = void>
using BackendResult = std::expected<T, BackendError>;

namespace BackendFeature {
inline constexpr auto IS_SUPPORTED_AT_RUNTIME = (1ULL << 0);
inline constexpr auto SUPPORTS_SPEAK = (1ULL << 2);
inline constexpr auto SUPPORTS_SPEAK_TO_MEMORY = (1ULL << 3);
inline constexpr auto SUPPORTS_BRAILLE = (1ULL << 4);
inline constexpr auto SUPPORTS_OUTPUT = (1ULL << 5);
inline constexpr auto SUPPORTS_IS_SPEAKING = (1ULL << 6);
inline constexpr auto SUPPORTS_STOP = (1ULL << 7);
inline constexpr auto SUPPORTS_PAUSE = (1ULL << 8);
inline constexpr auto SUPPORTS_RESUME = (1ULL << 9);
inline constexpr auto SUPPORTS_SET_VOLUME = (1ULL << 10);
inline constexpr auto SUPPORTS_GET_VOLUME = (1ULL << 11);
inline constexpr auto SUPPORTS_SET_RATE = (1ULL << 12);
inline constexpr auto SUPPORTS_GET_RATE = (1ULL << 13);
inline constexpr auto SUPPORTS_SET_PITCH = (1ULL << 14);
inline constexpr auto SUPPORTS_GET_PITCH = (1ULL << 15);
inline constexpr auto SUPPORTS_REFRESH_VOICES = (1ULL << 16);
inline constexpr auto SUPPORTS_COUNT_VOICES = (1ULL << 17);
inline constexpr auto SUPPORTS_GET_VOICE_NAME = (1ULL << 18);
inline constexpr auto SUPPORTS_GET_VOICE_LANGUAGE = (1ULL << 19);
inline constexpr auto SUPPORTS_GET_VOICE = (1ULL << 20);
inline constexpr auto SUPPORTS_SET_VOICE = (1ULL << 21);
inline constexpr auto SUPPORTS_GET_CHANNELS = (1ULL << 22);
inline constexpr auto SUPPORTS_GET_SAMPLE_RATE = (1ULL << 23);
inline constexpr auto SUPPORTS_GET_BIT_DEPTH = (1ULL << 24);
inline constexpr auto PERFORMS_SILENCE_TRIMMING_ON_SPEAK = (1ULL << 25);
inline constexpr auto PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY =
    (1ULL << 26);
inline constexpr auto SUPPORTS_SPEAK_SSML = (1ULL << 27);
inline constexpr auto SUPPORTS_SPEAK_TO_MEMORY_SSML = (1ULL << 28);
} // namespace BackendFeature

class TextToSpeechBackend {
#ifdef __ANDROID__
protected:
  JavaVM *java_vm{nullptr};
#endif
#ifdef _WIN32
protected:
  HWND hwnd_in{static_cast<HWND>(INVALID_HANDLE_VALUE)};
#endif
public:
  using AudioCallback = std::function<void(void *, const float *, std::size_t,
                                           std::size_t, std::size_t)>;
  virtual ~TextToSpeechBackend() = default;
  [[nodiscard]] virtual std::string_view get_name() const = 0;
  [[nodiscard]] virtual std::bitset<64> get_features() const = 0;
  virtual BackendResult<> initialize() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> speak([[maybe_unused]] std::string_view text,
                                [[maybe_unused]] bool interrupt) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<>
  speak_to_memory([[maybe_unused]] std::string_view text,
                  [[maybe_unused]] AudioCallback callback,
                  [[maybe_unused]] void *userdata) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> braille([[maybe_unused]] std::string_view text) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> output([[maybe_unused]] std::string_view text,
                                 [[maybe_unused]] bool interrupt) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<bool> is_speaking() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> stop() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> pause() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> resume() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> set_volume([[maybe_unused]] float volume) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<float> get_volume() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> set_rate([[maybe_unused]] float rate) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<float> get_rate() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> set_pitch([[maybe_unused]] float pitch) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<float> get_pitch() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> refresh_voices() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<std::size_t> count_voices() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<std::string>
  get_voice_name([[maybe_unused]] std::size_t id) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<std::string>
  get_voice_language([[maybe_unused]] std::size_t id) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> set_voice([[maybe_unused]] std::size_t id) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<std::size_t> get_voice() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<std::size_t> get_channels() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<std::size_t> get_sample_rate() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<std::size_t> get_bit_depth() {
    return std::unexpected(BackendError::NotImplemented);
  }
#ifdef __ANDROID__
  virtual void set_java_vm(JavaVM *vm) final { this->java_vm = vm; }
#endif
#ifdef _WIN32
  virtual void set_hwnd(HWND hwnd) final { this->hwnd_in = hwnd; }
#endif
};
