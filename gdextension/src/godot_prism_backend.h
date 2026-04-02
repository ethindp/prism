#pragma once

#include "godot_cpp/classes/audio_stream_wav.hpp"
#include "godot_cpp/classes/ref.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/classes/wrapped.hpp"
#include "godot_cpp/variant/callable.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_float32_array.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/typed_array.hpp"
#include "godot_cpp/variant/variant.hpp"

#include <cstddef>
#include <cstdint>
#include <prism.h>

using namespace godot;

class GodotPrismBackend : public RefCounted {
  GDCLASS(GodotPrismBackend, RefCounted)

  friend class GodotPrismContext;

private:
  PrismBackend *backend = nullptr;
  std::uint64_t features = 0;

  void _init_raw(PrismBackend *raw);

protected:
  static void _bind_methods();

public:
  GodotPrismBackend() = default;
  ~GodotPrismBackend() override;

  bool is_valid() const;

  BitField<PrismBackendFeature> get_features() const;
  String get_name() const;
  PrismError speak(const String &text, bool interrupt = true);
  PrismError speak_to_memory(const String &text, const Callable &callback);
  PrismError braille(const String &text);
  PrismError output(const String &text, bool interrupt = true);
  PrismError stop();
  PrismError pause();
  PrismError resume();
  bool get_speaking() const;
  void set_volume(float v);
  float get_volume() const;
  void set_pitch(float v);
  float get_pitch() const;
  void set_rate(float v);
  float get_rate() const;
  PrismError refresh_voices();
  std::int64_t get_voices_count() const;
  String get_voice_name(std::int64_t index) const;
  String get_voice_language(std::int64_t index) const;
  void set_voice(std::int64_t index);
  std::int64_t get_voice() const;
  std::int64_t get_channels() const;
  std::int64_t get_sample_rate() const;
  std::int64_t get_bit_depth() const;
  Ref<AudioStreamWAV> speak_to_stream(const String &text);
  bool has_feature(BitField<PrismBackendFeature> flag) const;
  TypedArray<Dictionary> get_voices() const;
  std::int64_t find_voice(const String &pattern) const;
  std::int64_t find_voice_by_language(const String &prefix) const;
};

VARIANT_ENUM_CAST(PrismError);
VARIANT_BITFIELD_CAST(PrismBackendFeature);
