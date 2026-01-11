// SPDX-License-Identifier: MPL-2.0

#pragma once
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#ifdef __ANDROID__
#include <jni.h>
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
  Unknown
};

template <typename T = void>
using BackendResult = std::expected<T, BackendError>;

class TextToSpeechBackend {
#ifdef __ANDROID__
protected:
  JavaVM *java_vm{nullptr};
#endif
public:
  using AudioCallback = std::function<void(void *, const float *, std::size_t,
                                           std::size_t, std::size_t)>;
  virtual ~TextToSpeechBackend() = default;
  virtual std::string_view get_name() const = 0;
  virtual BackendResult<> initialize() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> speak(std::string_view text, bool interrupt) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> speak_to_memory(std::string_view text,
                                          AudioCallback callback,
                                          void *userdata) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> braille(std::string_view text) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> output(std::string_view text, bool interrupt) {
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
  virtual BackendResult<> set_volume(float volume) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<float> get_volume() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> set_rate(float rate) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<float> get_rate() {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> set_pitch(float pitch) {
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
  virtual BackendResult<std::string> get_voice_name(std::size_t id) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<std::string> get_voice_language(std::size_t id) {
    return std::unexpected(BackendError::NotImplemented);
  }
  virtual BackendResult<> set_voice(std::size_t id) {
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
  virtual void set_java_vm(JavaVM *vm) { this->java_vm = vm; }
#endif
};
