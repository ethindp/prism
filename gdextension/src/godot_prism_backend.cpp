#include "godot_prism_backend.h"

#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/utility_functions.hpp"

#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

struct SpeakToMemoryAccumulator {
  std::vector<float> samples;
  std::size_t channels = 0;
  std::size_t sample_rate = 0;
};

static void speak_to_memory_trampoline(void *userdata,
                                       const float *PRISM_RESTRICT samples,
                                       std::size_t sample_count,
                                       std::size_t channels,
                                       std::size_t sample_rate) {
  auto *acc = static_cast<SpeakToMemoryAccumulator *>(userdata);
  acc->channels = channels;
  acc->sample_rate = sample_rate;
  acc->samples.insert(acc->samples.end(), samples, samples + sample_count);
}

static PrismError _synthesise(PrismBackend *backend, const String &text,
                              SpeakToMemoryAccumulator &acc) {
  return prism_backend_speak_to_memory(backend, text.utf8().get_data(),
                                       speak_to_memory_trampoline, &acc);
}

void GodotPrismBackend::_init_raw(PrismBackend *raw) {
  assert(raw);
  backend = raw;
  features = prism_backend_get_features(backend);
  auto const res = prism_backend_initialize(backend);
  if (res != PRISM_OK && res != PRISM_ERROR_ALREADY_INITIALIZED) {
    UtilityFunctions::push_error(
        vformat("Prism: Could not initialise backend '%s': %s",
                String::utf8(prism_backend_name(backend)),
                String::utf8(prism_error_string(res))));
    return;
  }
  features = prism_backend_get_features(backend);
}

GodotPrismBackend::~GodotPrismBackend() {
  if (backend != nullptr) {
    if (features & PRISM_BACKEND_SUPPORTS_STOP) {
      prism_backend_stop(backend);
    }
    prism_backend_free(backend);
    backend = nullptr;
  }
}

bool GodotPrismBackend::is_valid() const { return backend != nullptr; }

BitField<PrismBackendFeature> GodotPrismBackend::get_features() const {
  return BitField<PrismBackendFeature>(features);
}

String GodotPrismBackend::get_name() const {
  if (backend == nullptr) {
    return String();
  }
  return String::utf8(prism_backend_name(backend));
}

PrismError GodotPrismBackend::speak(const String &text, bool interrupt) {
  if (backend == nullptr) {
    return PRISM_ERROR_NOT_INITIALIZED;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_SPEAK) == 0) {
    return PRISM_ERROR_NOT_IMPLEMENTED;
  }
  if (text.is_empty()) {
    return PRISM_ERROR_INVALID_PARAM;
  }
  return prism_backend_speak(backend, text.utf8().get_data(), interrupt);
}

PrismError GodotPrismBackend::speak_to_memory(const String &text,
                                              const Callable &callback) {
  if (backend == nullptr) {
    return PRISM_ERROR_NOT_INITIALIZED;
  }
  if (callback.is_null() || !callback.is_valid()) {
    return PRISM_ERROR_INVALID_PARAM;
  }
  if (text.is_empty()) {
    return PRISM_ERROR_INVALID_PARAM;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY) == 0) {
    return PRISM_ERROR_NOT_IMPLEMENTED;
  }
  SpeakToMemoryAccumulator acc;
  auto const err = _synthesise(backend, text, acc);
  if (err != PRISM_OK) {
    return err;
  }
  PackedFloat32Array samples_out;
  if (!acc.samples.empty()) {
    samples_out.resize(static_cast<std::int64_t>(acc.samples.size()));
    std::memcpy(samples_out.ptrw(), acc.samples.data(),
                acc.samples.size() * sizeof(float));
  }
  Array args;
  args.push_back(samples_out);
  args.push_back(static_cast<std::int64_t>(acc.channels));
  args.push_back(static_cast<std::int64_t>(acc.sample_rate));
  callback.callv(args);
  return PRISM_OK;
}

PrismError GodotPrismBackend::braille(const String &text) {
  if (backend == nullptr) {
    return PRISM_ERROR_NOT_INITIALIZED;
  }
  if (text.is_empty()) {
    return PRISM_ERROR_INVALID_PARAM;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_BRAILLE) == 0) {
    return PRISM_ERROR_NOT_IMPLEMENTED;
  }
  return prism_backend_braille(backend, text.utf8().get_data());
}

PrismError GodotPrismBackend::output(const String &text, bool interrupt) {
  if (backend == nullptr) {
    return PRISM_ERROR_NOT_INITIALIZED;
  }
  if (text.is_empty()) {
    return PRISM_ERROR_INVALID_PARAM;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_OUTPUT) == 0) {
    return PRISM_ERROR_NOT_IMPLEMENTED;
  }
  return prism_backend_output(backend, text.utf8().get_data(), interrupt);
}

PrismError GodotPrismBackend::stop() {
  if (backend == nullptr) {
    return PRISM_ERROR_NOT_INITIALIZED;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_STOP) == 0) {
    return PRISM_ERROR_NOT_IMPLEMENTED;
  }
  return prism_backend_stop(backend);
}

PrismError GodotPrismBackend::pause() {
  if (backend == nullptr) {
    return PRISM_ERROR_NOT_INITIALIZED;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_PAUSE) == 0) {
    return PRISM_ERROR_NOT_IMPLEMENTED;
  }
  return prism_backend_pause(backend);
}

PrismError GodotPrismBackend::resume() {
  if (backend == nullptr) {
    return PRISM_ERROR_NOT_INITIALIZED;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_RESUME) == 0) {
    return PRISM_ERROR_NOT_IMPLEMENTED;
  }
  return prism_backend_resume(backend);
}

bool GodotPrismBackend::get_speaking() const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_IS_SPEAKING) == 0) {
    return false;
  }
  bool speaking = false;
  auto const err = prism_backend_is_speaking(backend, &speaking);
  return (err == PRISM_OK) && speaking;
}

void GodotPrismBackend::set_volume(float v) {
  if (backend == nullptr) {
    UtilityFunctions::push_error("Prism: set_volume called on invalid backend");
    return;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_SET_VOLUME) == 0) {
    UtilityFunctions::push_error("Prism: backend does not support set_volume");
    return;
  }
  auto const err = prism_backend_set_volume(backend, v);
  if (err != PRISM_OK) {
    UtilityFunctions::push_error(vformat(
        "Prism: set_volume failed: %s", String::utf8(prism_error_string(err))));
  }
}

float GodotPrismBackend::get_volume() const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_GET_VOLUME) == 0) {
    return 0.5f;
  }
  float v = 0.5f;
  auto const err = prism_backend_get_volume(backend, &v);
  if (err != PRISM_OK) {
    UtilityFunctions::push_error(vformat(
        "Prism: get_volume failed: %s", String::utf8(prism_error_string(err))));
    return std::numeric_limits<float>::quiet_NaN();
  }
  return v;
}

void GodotPrismBackend::set_pitch(float v) {
  if (backend == nullptr) {
    UtilityFunctions::push_error("Prism: set_pitch called on invalid backend");
    return;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_SET_PITCH) == 0) {
    UtilityFunctions::push_error("Prism: backend does not support set_pitch");
    return;
  }
  auto const err = prism_backend_set_pitch(backend, v);
  if (err != PRISM_OK) {
    UtilityFunctions::push_error(vformat(
        "Prism: set_pitch failed: %s", String::utf8(prism_error_string(err))));
  }
}

float GodotPrismBackend::get_pitch() const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_GET_PITCH) == 0) {
    return 0.5f;
  }
  float v = 0.5f;
  auto const err = prism_backend_get_pitch(backend, &v);
  if (err != PRISM_OK) {
    UtilityFunctions::push_error(vformat(
        "Prism: get_pitch failed: %s", String::utf8(prism_error_string(err))));
    return std::numeric_limits<float>::quiet_NaN();
  }
  return v;
}

void GodotPrismBackend::set_rate(float v) {
  if (backend == nullptr) {
    UtilityFunctions::push_error("Prism: set_rate called on invalid backend");
    return;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_SET_RATE) == 0) {
    UtilityFunctions::push_error("Prism: backend does not support set_rate");
    return;
  }
  auto const err = prism_backend_set_rate(backend, v);
  if (err != PRISM_OK) {
    UtilityFunctions::push_error(vformat(
        "Prism: set_rate failed: %s", String::utf8(prism_error_string(err))));
  }
}

float GodotPrismBackend::get_rate() const {
  if (backend == nullptr || (features & PRISM_BACKEND_SUPPORTS_GET_RATE) == 0) {
    return 0.5f;
  }
  float v = 0.5f;
  auto const err = prism_backend_get_rate(backend, &v);
  if (err != PRISM_OK) {
    UtilityFunctions::push_error(vformat(
        "Prism: get_rate failed: %s", String::utf8(prism_error_string(err))));
    return std::numeric_limits<float>::quiet_NaN();
  }
  return v;
}

PrismError GodotPrismBackend::refresh_voices() {
  if (backend == nullptr) {
    return PRISM_ERROR_NOT_INITIALIZED;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_REFRESH_VOICES) == 0) {
    return PRISM_ERROR_NOT_IMPLEMENTED;
  }
  return prism_backend_refresh_voices(backend);
}

std::int64_t GodotPrismBackend::get_voices_count() const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_COUNT_VOICES) == 0) {
    return 0;
  }
  std::size_t count = 0;
  auto const err = prism_backend_count_voices(backend, &count);
  return (err == PRISM_OK) ? static_cast<std::int64_t>(count) : 0;
}

String GodotPrismBackend::get_voice_name(std::int64_t index) const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_GET_VOICE_NAME) == 0) {
    return String();
  }
  const char *name = nullptr;
  auto const err = prism_backend_get_voice_name(
      backend, static_cast<std::size_t>(index), &name);
  if (err != PRISM_OK || name == nullptr) {
    return String();
  }
  return String::utf8(name);
}

String GodotPrismBackend::get_voice_language(std::int64_t index) const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_GET_VOICE_LANGUAGE) == 0) {
    return String();
  }
  const char *lang = nullptr;
  auto const err = prism_backend_get_voice_language(
      backend, static_cast<std::size_t>(index), &lang);
  if (err != PRISM_OK || lang == nullptr) {
    return String();
  }
  return String::utf8(lang);
}

void GodotPrismBackend::set_voice(std::int64_t index) {
  if (backend == nullptr) {
    UtilityFunctions::push_error("Prism: set_voice called on invalid backend");
    return;
  }
  if ((features & PRISM_BACKEND_SUPPORTS_SET_VOICE) == 0) {
    UtilityFunctions::push_error("Prism: backend does not support set_voice");
    return;
  }
  auto const err =
      prism_backend_set_voice(backend, static_cast<std::size_t>(index));
  if (err != PRISM_OK) {
    UtilityFunctions::push_error(vformat(
        "Prism: set_voice failed: %s", String::utf8(prism_error_string(err))));
  }
}

std::int64_t GodotPrismBackend::get_voice() const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_GET_VOICE) == 0) {
    return -1;
  }
  std::size_t voice_id = 0;
  auto const err = prism_backend_get_voice(backend, &voice_id);
  return (err == PRISM_OK) ? static_cast<int>(voice_id) : -1;
}

std::int64_t GodotPrismBackend::get_channels() const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_GET_CHANNELS) == 0) {
    return -1;
  }
  std::size_t ch = 0;
  auto const err = prism_backend_get_channels(backend, &ch);
  return (err == PRISM_OK) ? static_cast<int>(ch) : -1;
}

std::int64_t GodotPrismBackend::get_sample_rate() const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_GET_SAMPLE_RATE) == 0) {
    return -1;
  }
  std::size_t sr = 0;
  auto const err = prism_backend_get_sample_rate(backend, &sr);
  return (err == PRISM_OK) ? static_cast<int>(sr) : -1;
}

std::int64_t GodotPrismBackend::get_bit_depth() const {
  if (backend == nullptr ||
      (features & PRISM_BACKEND_SUPPORTS_GET_BIT_DEPTH) == 0) {
    return -1;
  }
  std::size_t bd = 0;
  auto const err = prism_backend_get_bit_depth(backend, &bd);
  return (err == PRISM_OK) ? static_cast<int>(bd) : -1;
}

Ref<AudioStreamWAV> GodotPrismBackend::speak_to_stream(const String &text) {
  if (backend == nullptr) {
    UtilityFunctions::push_error(
        "Prism: speak_to_stream called on invalid backend");
    return {};
  }
  if (text.is_empty()) {
    return {};
  }
  if ((features & PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY) == 0) {
    UtilityFunctions::push_error(
        "Prism: backend does not support speak_to_memory");
    return {};
  }
  SpeakToMemoryAccumulator acc;
  auto const err = _synthesise(backend, text, acc);
  if (err != PRISM_OK) {
    UtilityFunctions::push_error(
        vformat("Prism: speak_to_stream synthesis failed: %s",
                String::utf8(prism_error_string(err))));
    return {};
  }
  if (acc.samples.empty() || acc.channels == 0 || acc.sample_rate == 0) {
    return {};
  }
  PackedByteArray pcm;
  pcm.resize(static_cast<std::int64_t>(acc.samples.size()) * 2);
  uint8_t *dst = pcm.ptrw();
  for (std::size_t i = 0; i < acc.samples.size(); ++i) {
    float s = acc.samples[i];
    auto v = static_cast<std::int16_t>(s * 32767.0f);
    dst[i * 2 + 0] = static_cast<uint8_t>(v & 0xFF);
    dst[i * 2 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  }
  Ref<AudioStreamWAV> stream;
  stream.instantiate();
  stream->set_format(AudioStreamWAV::FORMAT_16_BITS);
  stream->set_mix_rate(static_cast<int>(acc.sample_rate));
  stream->set_stereo(acc.channels > 1);
  stream->set_data(pcm);
  return stream;
}

bool GodotPrismBackend::has_feature(BitField<PrismBackendFeature> flag) const {
  return (features & static_cast<std::uint64_t>(flag)) ==
         static_cast<std::uint64_t>(flag);
}

TypedArray<Dictionary> GodotPrismBackend::get_voices() const {
  TypedArray<Dictionary> result;
  auto const count = get_voices_count();
  for (std::int64_t i = 0; i < count; ++i) {
    Dictionary d;
    d["index"] = i;
    d["name"] = get_voice_name(i);
    d["language"] = get_voice_language(i);
    result.push_back(d);
  }
  return result;
}

std::int64_t GodotPrismBackend::find_voice(const String &pattern) const {
  if (pattern.is_empty()) {
    return -1;
  }
  auto const lower = pattern.to_lower();
  auto const count = get_voices_count();
  for (std::int64_t i = 0; i < count; ++i) {
    if (get_voice_name(i).to_lower().find(lower) != -1) {
      return i;
    }
  }
  return -1;
}

std::int64_t
GodotPrismBackend::find_voice_by_language(const String &prefix) const {
  if (prefix.is_empty()) {
    return -1;
  }
  auto const lower = prefix.to_lower();
  auto const count = get_voices_count();
  for (std::int64_t i = 0; i < count; ++i) {
    if (get_voice_language(i).to_lower().begins_with(lower)) {
      return i;
    }
  }
  return -1;
}

void GodotPrismBackend::_bind_methods() {
  BIND_ENUM_CONSTANT(PRISM_OK);
  BIND_ENUM_CONSTANT(PRISM_ERROR_NOT_INITIALIZED);
  BIND_ENUM_CONSTANT(PRISM_ERROR_INVALID_PARAM);
  BIND_ENUM_CONSTANT(PRISM_ERROR_NOT_IMPLEMENTED);
  BIND_ENUM_CONSTANT(PRISM_ERROR_NO_VOICES);
  BIND_ENUM_CONSTANT(PRISM_ERROR_VOICE_NOT_FOUND);
  BIND_ENUM_CONSTANT(PRISM_ERROR_SPEAK_FAILURE);
  BIND_ENUM_CONSTANT(PRISM_ERROR_MEMORY_FAILURE);
  BIND_ENUM_CONSTANT(PRISM_ERROR_RANGE_OUT_OF_BOUNDS);
  BIND_ENUM_CONSTANT(PRISM_ERROR_INTERNAL);
  BIND_ENUM_CONSTANT(PRISM_ERROR_NOT_SPEAKING);
  BIND_ENUM_CONSTANT(PRISM_ERROR_NOT_PAUSED);
  BIND_ENUM_CONSTANT(PRISM_ERROR_ALREADY_PAUSED);
  BIND_ENUM_CONSTANT(PRISM_ERROR_INVALID_UTF8);
  BIND_ENUM_CONSTANT(PRISM_ERROR_INVALID_OPERATION);
  BIND_ENUM_CONSTANT(PRISM_ERROR_ALREADY_INITIALIZED);
  BIND_ENUM_CONSTANT(PRISM_ERROR_BACKEND_NOT_AVAILABLE);
  BIND_ENUM_CONSTANT(PRISM_ERROR_UNKNOWN);
  BIND_ENUM_CONSTANT(PRISM_ERROR_INVALID_AUDIO_FORMAT);
  BIND_ENUM_CONSTANT(PRISM_ERROR_INTERNAL_BACKEND_LIMIT_EXCEEDED);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_SPEAK);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_BRAILLE);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_OUTPUT);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_IS_SPEAKING);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_STOP);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_PAUSE);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_RESUME);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_SET_VOLUME);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_GET_VOLUME);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_SET_RATE);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_GET_RATE);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_SET_PITCH);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_GET_PITCH);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_REFRESH_VOICES);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_COUNT_VOICES);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_GET_VOICE_NAME);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_GET_VOICE_LANGUAGE);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_GET_VOICE);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_SET_VOICE);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_GET_CHANNELS);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_GET_SAMPLE_RATE);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_GET_BIT_DEPTH);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK);
  BIND_BITFIELD_FLAG(
      PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_SPEAK_SSML);
  BIND_BITFIELD_FLAG(PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY_SSML);
  ClassDB::bind_method(D_METHOD("is_valid"), &GodotPrismBackend::is_valid);
  ClassDB::bind_method(D_METHOD("speak", "text", "interrupt"),
                       &GodotPrismBackend::speak, DEFVAL(true));
  ClassDB::bind_method(D_METHOD("speak_to_memory", "text", "callback"),
                       &GodotPrismBackend::speak_to_memory);
  ClassDB::bind_method(D_METHOD("braille", "text"),
                       &GodotPrismBackend::braille);
  ClassDB::bind_method(D_METHOD("output", "text", "interrupt"),
                       &GodotPrismBackend::output, DEFVAL(true));
  ClassDB::bind_method(D_METHOD("stop"), &GodotPrismBackend::stop);
  ClassDB::bind_method(D_METHOD("pause"), &GodotPrismBackend::pause);
  ClassDB::bind_method(D_METHOD("resume"), &GodotPrismBackend::resume);
  ClassDB::bind_method(D_METHOD("refresh_voices"),
                       &GodotPrismBackend::refresh_voices);
  ClassDB::bind_method(D_METHOD("get_voice_name", "index"),
                       &GodotPrismBackend::get_voice_name);
  ClassDB::bind_method(D_METHOD("get_voice_language", "index"),
                       &GodotPrismBackend::get_voice_language);
  ClassDB::bind_method(D_METHOD("speak_to_stream", "text"),
                       &GodotPrismBackend::speak_to_stream);
  ClassDB::bind_method(D_METHOD("has_feature", "flag"),
                       &GodotPrismBackend::has_feature);
  ClassDB::bind_method(D_METHOD("get_voices"), &GodotPrismBackend::get_voices);
  ClassDB::bind_method(D_METHOD("find_voice", "pattern"),
                       &GodotPrismBackend::find_voice);
  ClassDB::bind_method(D_METHOD("find_voice_by_language", "prefix"),
                       &GodotPrismBackend::find_voice_by_language);
  ClassDB::bind_method(D_METHOD("get_features"),
                       &GodotPrismBackend::get_features);
  ClassDB::bind_method(D_METHOD("get_name"), &GodotPrismBackend::get_name);
  ClassDB::bind_method(D_METHOD("get_speaking"),
                       &GodotPrismBackend::get_speaking);
  ClassDB::bind_method(D_METHOD("set_volume", "volume"),
                       &GodotPrismBackend::set_volume);
  ClassDB::bind_method(D_METHOD("get_volume"), &GodotPrismBackend::get_volume);
  ClassDB::bind_method(D_METHOD("set_pitch", "pitch"),
                       &GodotPrismBackend::set_pitch);
  ClassDB::bind_method(D_METHOD("get_pitch"), &GodotPrismBackend::get_pitch);
  ClassDB::bind_method(D_METHOD("set_rate", "rate"),
                       &GodotPrismBackend::set_rate);
  ClassDB::bind_method(D_METHOD("get_rate"), &GodotPrismBackend::get_rate);
  ClassDB::bind_method(D_METHOD("set_voice", "index"),
                       &GodotPrismBackend::set_voice);
  ClassDB::bind_method(D_METHOD("get_voice"), &GodotPrismBackend::get_voice);
  ClassDB::bind_method(D_METHOD("get_voices_count"),
                       &GodotPrismBackend::get_voices_count);
  ClassDB::bind_method(D_METHOD("get_channels"),
                       &GodotPrismBackend::get_channels);
  ClassDB::bind_method(D_METHOD("get_sample_rate"),
                       &GodotPrismBackend::get_sample_rate);
  ClassDB::bind_method(D_METHOD("get_bit_depth"),
                       &GodotPrismBackend::get_bit_depth);
  ADD_GROUP("Identity", "");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "features"), "", "get_features");
  ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "", "get_name");
  ADD_GROUP("State", "");
  ADD_PROPERTY(PropertyInfo(Variant::BOOL, "speaking"), "", "get_speaking");
  ADD_GROUP("Voice Parameters", "");
  ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "volume", PROPERTY_HINT_RANGE,
                            "0.0,1.0,0.01"),
               "set_volume", "get_volume");
  ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pitch", PROPERTY_HINT_RANGE,
                            "0.0,1.0,0.01"),
               "set_pitch", "get_pitch");
  ADD_PROPERTY(
      PropertyInfo(Variant::FLOAT, "rate", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"),
      "set_rate", "get_rate");
  ADD_GROUP("Voice Selection", "");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "voice"), "set_voice", "get_voice");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "voices_count"), "",
               "get_voices_count");
  ADD_GROUP("Audio Format", "");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "channels"), "", "get_channels");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "sample_rate"), "",
               "get_sample_rate");
  ADD_PROPERTY(PropertyInfo(Variant::INT, "bit_depth"), "", "get_bit_depth");
}
