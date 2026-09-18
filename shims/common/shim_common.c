// SPDX-License-Identifier: MPL-2.0
#include "shim_common.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

struct ShimString {
  ShimString *next;
  char *utf8;
  wchar_t *wide;
  char *ansi;
};

void shim_flag_raise(ShimFlag *flag) {
#ifdef _WIN32
  (void)InterlockedExchange(&flag->value, 1);
#else
  __atomic_store_n(&flag->value, 1, __ATOMIC_RELEASE);
#endif
}

bool shim_flag_take(ShimFlag *flag) {
#ifdef _WIN32
  return InterlockedExchange(&flag->value, 0) != 0;
#else
  return __atomic_exchange_n(&flag->value, 0, __ATOMIC_ACQ_REL) != 0;
#endif
}

static void PRISM_CALL shim_on_availability(void *userdata,
                                            PrismBackendId backend,
                                            const char *name, bool available) {
  (void)backend;
  (void)name;
  (void)available;
  shim_flag_raise(userdata);
}

static void PRISM_CALL shim_on_baseline(void *userdata) {
  shim_flag_raise(userdata);
}

PrismContext *shim_context_open(ShimFlag *stale) {
  PrismConfig config = prism_config_init();
  config.availability_callback = shim_on_availability;
  config.availability_baseline_callback = shim_on_baseline;
  config.availability_userdata = stale;
  config.availability_backoff_max_ms = 10000;
  config.availability_auto_power_manage = true;
  return prism_init(&config);
}

PrismBackendId shim_native_tts_id(void) {
#ifdef _WIN32
  return PRISM_BACKEND_SAPI;
#elifdef __APPLE__
  return PRISM_BACKEND_AV_SPEECH;
#elifdef __ANDROID__
  return PRISM_BACKEND_ANDROID_TTS;
#elifdef __EMSCRIPTEN__
  return PRISM_BACKEND_WEB_SPEECH;
#else
  return PRISM_BACKEND_SPEECH_DISPATCHER;
#endif
}

bool shim_error_means_lost(PrismError error) {
  switch (error) {
  case PRISM_ERROR_NOT_INITIALIZED:
  case PRISM_ERROR_SPEAK_FAILURE:
  case PRISM_ERROR_INTERNAL:
  case PRISM_ERROR_BACKEND_NOT_AVAILABLE:
  case PRISM_ERROR_UNKNOWN:
  case PRISM_ERROR_BACKEND_ENTERED_UNDEFINED_STATE:
    return true;
  default:
    return false;
  }
}

bool shim_backend_live(PrismBackend *backend) {
  return backend != PRISM_SHIM_NULL &&
         (prism_backend_get_features(backend) &
          PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME) != 0;
}

PrismBackend *shim_create_live(PrismContext *ctx, PrismBackendId id) {
  if (ctx == PRISM_SHIM_NULL || id == PRISM_BACKEND_INVALID ||
      !prism_registry_exists(ctx, id)) {
    return PRISM_SHIM_NULL;
  }
  PrismBackend *backend = prism_registry_create(ctx, id);
  if (backend == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  if (!shim_backend_live(backend)) {
    prism_backend_free(backend);
    return PRISM_SHIM_NULL;
  }
  const PrismError error = prism_backend_initialize(backend);
  if (error != PRISM_OK && error != PRISM_ERROR_ALREADY_INITIALIZED) {
    prism_backend_free(backend);
    return PRISM_SHIM_NULL;
  }
  return backend;
}

PrismBackend *shim_slot_get(PrismContext *ctx, PrismBackendId id,
                            PrismBackend **slot) {
  if (*slot == PRISM_SHIM_NULL) {
    *slot = shim_create_live(ctx, id);
  }
  return *slot;
}

bool shim_slot_live(PrismContext *ctx, PrismBackendId id, PrismBackend **slot) {
  if (*slot != PRISM_SHIM_NULL) {
    if (shim_backend_live(*slot)) {
      return true;
    }
    shim_slot_drop(slot);
  }
  *slot = shim_create_live(ctx, id);
  return *slot != PRISM_SHIM_NULL;
}

void shim_slot_drop(PrismBackend **slot) {
  if (*slot != PRISM_SHIM_NULL) {
    prism_backend_free(*slot);
    *slot = PRISM_SHIM_NULL;
  }
}

void shim_selection_clear(ShimSelection *selection) {
  shim_slot_drop(&selection->backend);
  selection->id = PRISM_BACKEND_INVALID;
}

void shim_select(PrismContext *ctx, ShimSelection *selection,
                 ShimAcceptFn accept, void *userdata) {
  if (ctx == PRISM_SHIM_NULL) {
    shim_selection_clear(selection);
    return;
  }
  const size_t count = prism_registry_count(ctx);
  for (size_t i = 0; i < count; ++i) {
    const PrismBackendId id = prism_registry_id_at(ctx, i);
    if (accept != PRISM_SHIM_NULL && !accept(id, userdata)) {
      continue;
    }
    if (selection->backend != PRISM_SHIM_NULL && selection->id == id) {
      if (shim_backend_live(selection->backend)) {
        return;
      }
      shim_selection_clear(selection);
      continue;
    }
    PrismBackend *backend = shim_create_live(ctx, id);
    if (backend != PRISM_SHIM_NULL) {
      shim_selection_clear(selection);
      selection->backend = backend;
      selection->id = id;
      return;
    }
  }
  shim_selection_clear(selection);
}

static ShimString *shim_intern(ShimStrings *strings, const char *utf8) {
  if (strings == PRISM_SHIM_NULL || utf8 == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  for (ShimString *entry = strings->head; entry != PRISM_SHIM_NULL;
       entry = entry->next) {
    if (strcmp(entry->utf8, utf8) == 0) {
      return entry;
    }
  }
  ShimString *entry = calloc(1, sizeof(*entry));
  if (entry == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  entry->utf8 = shim_strdup(utf8);
  if (entry->utf8 == PRISM_SHIM_NULL) {
    free(entry);
    return PRISM_SHIM_NULL;
  }
  entry->next = strings->head;
  strings->head = entry;
  return entry;
}

const wchar_t *shim_intern_wide(ShimStrings *strings, const char *utf8) {
  ShimString *entry = shim_intern(strings, utf8);
  if (entry == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  if (entry->wide == PRISM_SHIM_NULL) {
    entry->wide = shim_utf8_to_wchar(entry->utf8);
  }
  return entry->wide;
}

const char *shim_intern_utf8(ShimStrings *strings, const char *utf8) {
  ShimString *entry = shim_intern(strings, utf8);
  return entry == PRISM_SHIM_NULL ? PRISM_SHIM_NULL : entry->utf8;
}

const char *shim_intern_ansi(ShimStrings *strings, const char *utf8) {
  ShimString *entry = shim_intern(strings, utf8);
  if (entry == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  if (entry->ansi == PRISM_SHIM_NULL) {
    entry->ansi = shim_utf8_to_ansi(entry->utf8);
  }
  return entry->ansi;
}

void shim_strings_free(ShimStrings *strings) {
  ShimString *entry = strings->head;
  while (entry != PRISM_SHIM_NULL) {
    ShimString *next = entry->next;
    free(entry->utf8);
    free(entry->wide);
    free(entry->ansi);
    free(entry);
    entry = next;
  }
  strings->head = PRISM_SHIM_NULL;
}

char *shim_strdup(const char *text) {
  if (text == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  const size_t length = strlen(text);
  if (length == SIZE_MAX) {
    return PRISM_SHIM_NULL;
  }
  char *copy = malloc(length + 1);
  if (copy == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  for (size_t i = 0; i <= length; ++i) {
    copy[i] = text[i];
  }
  return copy;
}

char *shim_wchar_to_utf8(const wchar_t *src) {
  if (src == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  size_t in_len = 0;
  while (src[in_len] != L'\0') {
    ++in_len;
  }
  if (in_len > ((SIZE_MAX - 1) / 4)) {
    return PRISM_SHIM_NULL;
  }
  const size_t cap = (in_len * 4) + 1;
  char *buf = malloc(cap);
  if (buf == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  size_t in_pos = 0;
  size_t out_pos = 0;
  while (in_pos < in_len) {
    uint32_t cp = (uint32_t)src[in_pos];
    ++in_pos;
    if ((WCHAR_MAX <= 0xFFFF) && (cp >= 0xD800) && (cp <= 0xDBFF) &&
        (in_pos < in_len)) {
      const uint32_t low = (uint32_t)src[in_pos];
      if ((low >= 0xDC00) && (low <= 0xDFFF)) {
        ++in_pos;
        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
      }
    }
    if (((cp >= 0xD800) && (cp <= 0xDFFF)) || (cp > 0x10FFFF)) {
      cp = 0xFFFD;
    }
    if ((cap - out_pos) < 5) {
      free(buf);
      return PRISM_SHIM_NULL;
    }
    if (cp < 0x80) {
      buf[out_pos] = (char)cp;
      out_pos++;
    } else if (cp < 0x800) {
      buf[out_pos] = (char)(0xC0 | (cp >> 6));
      buf[out_pos + 1] = (char)(0x80 | (cp & 0x3F));
      out_pos += 2;
    } else if (cp < 0x10000) {
      buf[out_pos] = (char)(0xE0 | (cp >> 12));
      buf[out_pos + 1] = (char)(0x80 | ((cp >> 6) & 0x3F));
      buf[out_pos + 2] = (char)(0x80 | (cp & 0x3F));
      out_pos += 3;
    } else {
      buf[out_pos] = (char)(0xF0U | (cp >> 18U));
      buf[out_pos + 1U] = (char)(0x80U | ((cp >> 12U) & 0x3FU));
      buf[out_pos + 2U] = (char)(0x80U | ((cp >> 6U) & 0x3FU));
      buf[out_pos + 3U] = (char)(0x80U | (cp & 0x3FU));
      out_pos += 4U;
    }
  }
  buf[out_pos] = '\0';
  return buf;
}

wchar_t *shim_utf8_to_wchar(const char *src) {
  if (src == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  const size_t in_len = strlen(src);
  if (in_len > ((SIZE_MAX / sizeof(wchar_t)) - 1)) {
    return PRISM_SHIM_NULL;
  }
  const size_t cap = in_len + 1;
  wchar_t *buf = malloc(cap * sizeof(wchar_t));
  if (buf == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  const unsigned char *in = (const unsigned char *)src;
  size_t in_pos = 0;
  size_t out_pos = 0;
  while (in_pos < in_len) {
    const unsigned char lead = in[in_pos];
    uint32_t cp = 0;
    size_t trail = 0;
    if (lead < 0x80) {
      cp = (uint32_t)lead;
      trail = 0;
    } else if ((lead & 0xE0) == 0xC0) {
      cp = (uint32_t)(lead & 0x1F);
      trail = 1;
    } else if ((lead & 0xF0) == 0xE0) {
      cp = (uint32_t)(lead & 0x0F);
      trail = 2;
    } else if ((lead & 0xF8) == 0xF0) {
      cp = (uint32_t)(lead & 0x07);
      trail = 3;
    } else {
      free(buf);
      return PRISM_SHIM_NULL;
    }
    if ((in_len - in_pos) <= trail) {
      free(buf);
      return PRISM_SHIM_NULL;
    }
    for (size_t k = 1; k <= trail; ++k) {
      const unsigned char cont = in[in_pos + k];
      if ((cont & 0xC0) != 0x80) {
        free(buf);
        return PRISM_SHIM_NULL;
      }
      cp = (cp << 6) | (uint32_t)(cont & 0x3F);
    }
    in_pos += trail + 1;
    if (((trail == 1) && (cp < 0x80)) || ((trail == 2) && (cp < 0x800)) ||
        ((trail == 3) && (cp < 0x10000))) {
      free(buf);
      return PRISM_SHIM_NULL;
    }
    if ((cp > 0x10FFFF) || ((cp >= 0xD800) && (cp <= 0xDFFF))) {
      free(buf);
      return PRISM_SHIM_NULL;
    }
    if (WCHAR_MAX <= 0xFFFF) {
      if (cp >= 0x10000) {
        if ((cap - out_pos) < 3) {
          free(buf);
          return PRISM_SHIM_NULL;
        }
        const uint32_t adjusted = cp - 0x10000;
        buf[out_pos] = (wchar_t)(0xD800 + (adjusted >> 10));
        buf[out_pos + 1] = (wchar_t)(0xDC00 + (adjusted & 0x3FF));
        out_pos += 2;
        continue;
      }
    }
    if ((cap - out_pos) < 2) {
      free(buf);
      return PRISM_SHIM_NULL;
    }
    buf[out_pos] = (wchar_t)cp;
    out_pos++;
  }
  buf[out_pos] = L'\0';
  return buf;
}

char *shim_ansi_to_utf8(const char *text) {
  if (text == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
#ifdef _WIN32
  const int count = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, text, -1,
                                        PRISM_SHIM_NULL, 0);
  if (count <= 0) {
    return PRISM_SHIM_NULL;
  }
  wchar_t *wide = malloc((size_t)count * sizeof(*wide));
  if (wide == PRISM_SHIM_NULL ||
      MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, text, -1, wide,
                          count) == 0) {
    free(wide);
    return PRISM_SHIM_NULL;
  }
  char *result = shim_wchar_to_utf8(wide);
  free(wide);
  return result;
#else
  return shim_strdup(text);
#endif
}

char *shim_utf8_to_ansi(const char *text) {
  if (text == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
#ifdef _WIN32
  wchar_t *wide = shim_utf8_to_wchar(text);
  if (wide == PRISM_SHIM_NULL) {
    return PRISM_SHIM_NULL;
  }
  const int count = WideCharToMultiByte(CP_ACP, 0, wide, -1, PRISM_SHIM_NULL, 0,
                                        PRISM_SHIM_NULL, PRISM_SHIM_NULL);
  if (count <= 0) {
    free(wide);
    return PRISM_SHIM_NULL;
  }
  char *result = malloc((size_t)count);
  if (result == PRISM_SHIM_NULL ||
      WideCharToMultiByte(CP_ACP, 0, wide, -1, result, count, PRISM_SHIM_NULL,
                          PRISM_SHIM_NULL) == 0) {
    free(result);
    result = PRISM_SHIM_NULL;
  }
  free(wide);
  return result;
#else
  return shim_strdup(text);
#endif
}

float shim_clamp01(float value) {
  if (!(value >= 0.0F)) {
    return 0.0F;
  }
  if (value > 1.0F) {
    return 1.0F;
  }
  return value;
}

float shim_to_unit(float value, float min, float max) {
  return shim_clamp01((value - min) / (max - min));
}

float shim_from_unit(float value, float min, float max) {
  return min + (shim_clamp01(value) * (max - min));
}

void shim_audio_buffer_init(ShimAudioBuffer *buffer) {
  if (buffer != PRISM_SHIM_NULL) {
    *buffer = (ShimAudioBuffer){.samples = PRISM_SHIM_NULL};
  }
}

void shim_audio_buffer_clear(ShimAudioBuffer *buffer) {
  if (buffer == PRISM_SHIM_NULL) {
    return;
  }
  free(buffer->samples);
  *buffer = (ShimAudioBuffer){.samples = PRISM_SHIM_NULL};
}

void PRISM_CALL shim_audio_accumulate(void *userdata,
                                      const float *PRISM_RESTRICT samples,
                                      size_t sample_count, size_t channels,
                                      size_t sample_rate) {
  ShimAudioBuffer *buffer = userdata;
  if (buffer == PRISM_SHIM_NULL || samples == PRISM_SHIM_NULL ||
      sample_count == 0 || channels == 0 || sample_rate == 0 ||
      buffer->failed) {
    return;
  }
  if (buffer->sample_count != 0 &&
      (buffer->channels != channels || buffer->sample_rate != sample_rate)) {
    buffer->failed = true;
    return;
  }
  if (sample_count > SIZE_MAX - buffer->sample_count) {
    buffer->failed = true;
    return;
  }
  const size_t required = buffer->sample_count + sample_count;
  if (required > buffer->capacity) {
    size_t capacity = buffer->capacity == 0 ? 4096 : buffer->capacity;
    while (capacity < required) {
      if (capacity > SIZE_MAX / 2) {
        capacity = required;
        break;
      }
      capacity *= 2;
    }
    if (capacity > SIZE_MAX / sizeof(float)) {
      buffer->failed = true;
      return;
    }
    float *grown = realloc(buffer->samples, capacity * sizeof(float));
    if (grown == PRISM_SHIM_NULL) {
      buffer->failed = true;
      return;
    }
    buffer->samples = grown;
    buffer->capacity = capacity;
  }
  for (size_t i = 0; i < sample_count; ++i) {
    buffer->samples[buffer->sample_count + i] = samples[i];
  }
  buffer->sample_count = required;
  buffer->channels = channels;
  buffer->sample_rate = sample_rate;
}

bool shim_synthesize_audio(PrismBackend *backend, const char *text,
                           ShimAudioBuffer *buffer) {
  if (backend == PRISM_SHIM_NULL || text == PRISM_SHIM_NULL ||
      buffer == PRISM_SHIM_NULL) {
    return false;
  }
  shim_audio_buffer_clear(buffer);
  const PrismError error = prism_backend_speak_to_memory(
      backend, text, shim_audio_accumulate, buffer);
  if (error != PRISM_OK || buffer->failed || buffer->sample_count == 0 ||
      buffer->channels == 0 || buffer->sample_rate == 0) {
    shim_audio_buffer_clear(buffer);
    return false;
  }
  return true;
}

static float mono_frame(const ShimAudioBuffer *buffer, size_t frame) {
  const size_t base = frame * buffer->channels;
  float sum = 0.0F;
  for (size_t channel = 0; channel < buffer->channels; ++channel) {
    sum += buffer->samples[base + channel];
  }
  return sum / (float)buffer->channels;
}

bool shim_audio_to_pcm16_mono(const ShimAudioBuffer *buffer,
                              size_t target_sample_rate, int16_t **out_samples,
                              size_t *out_sample_count) {
  if (buffer == PRISM_SHIM_NULL || out_samples == PRISM_SHIM_NULL ||
      out_sample_count == PRISM_SHIM_NULL ||
      buffer->samples == PRISM_SHIM_NULL || buffer->channels == 0 ||
      buffer->sample_rate == 0 || buffer->sample_count < buffer->channels) {
    return false;
  }
  const size_t frames = buffer->sample_count / buffer->channels;
  if (target_sample_rate == 0) {
    target_sample_rate = buffer->sample_rate;
  }
  if (frames > SIZE_MAX / target_sample_rate) {
    return false;
  }
  const size_t output_frames =
      ((frames * target_sample_rate) + (buffer->sample_rate - 1U)) /
      buffer->sample_rate;
  if (output_frames == 0 || output_frames > SIZE_MAX / sizeof(int16_t)) {
    return false;
  }
  int16_t *output = malloc(output_frames * sizeof(*output));
  if (output == PRISM_SHIM_NULL) {
    return false;
  }
  for (size_t i = 0; i < output_frames; ++i) {
    const double position =
        ((double)i * (double)buffer->sample_rate) / (double)target_sample_rate;
    size_t first = (size_t)position;
    if (first >= frames) {
      first = frames - 1;
    }
    const size_t second = (first + 1U) < frames ? (first + 1U) : first;
    const float fraction = (float)(position - (double)first);
    float sample = (mono_frame(buffer, first) * (1.0F - fraction)) +
                   (mono_frame(buffer, second) * fraction);
    if (sample > 1.0F) {
      sample = 1.0F;
    } else if (sample < -1.0F) {
      sample = -1.0F;
    }
    output[i] =
        (int16_t)(sample >= 0.0F ? (sample * 32767.0F) : (sample * 32768.0F));
  }
  *out_samples = output;
  *out_sample_count = output_frames;
  return true;
}

static bool write_u16_le(FILE *file, uint16_t value) {
  const unsigned char bytes[2] = {(unsigned char)(value & 0xFF),
                                  (unsigned char)((value >> 8) & 0xFF)};
  return fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
}

static bool write_u32_le(FILE *file, uint32_t value) {
  const unsigned char bytes[4] = {(unsigned char)(value & 0xFF),
                                  (unsigned char)((value >> 8) & 0xFF),
                                  (unsigned char)((value >> 16) & 0xFF),
                                  (unsigned char)((value >> 24) & 0xFF)};
  return fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
}

bool shim_write_wav(const char *path, const ShimAudioBuffer *buffer) {
  if (path == PRISM_SHIM_NULL || buffer == PRISM_SHIM_NULL ||
      buffer->samples == PRISM_SHIM_NULL || buffer->channels == 0 ||
      buffer->sample_rate == 0 || buffer->channels > UINT16_MAX ||
      buffer->sample_rate > UINT32_MAX / (buffer->channels * 2) ||
      buffer->sample_count > (UINT32_MAX - 36) / 2) {
    return false;
  }
  const uint32_t data_size = (uint32_t)(buffer->sample_count * 2);
  FILE *file = fopen(path, "wb");
  if (file == PRISM_SHIM_NULL) {
    return false;
  }
  const uint16_t channels = (uint16_t)buffer->channels;
  const uint32_t sample_rate = (uint32_t)buffer->sample_rate;
  const uint16_t block_align = (uint16_t)(channels * 2);
  const uint32_t byte_rate = sample_rate * block_align;
  bool ok =
      (fwrite("RIFF", 1, 4, file) == 4 && write_u32_le(file, 36 + data_size) &&
       fwrite("WAVEfmt ", 1, 8, file) == 8 && write_u32_le(file, 16) &&
       write_u16_le(file, 1) && write_u16_le(file, channels) &&
       write_u32_le(file, sample_rate) && write_u32_le(file, byte_rate) &&
       write_u16_le(file, block_align) && write_u16_le(file, 16) &&
       fwrite("data", 1, 4, file) == 4 && write_u32_le(file, data_size)) != 0;
  for (size_t i = 0; ok && i < buffer->sample_count; ++i) {
    float sample = buffer->samples[i];
    if (sample > 1.0F) {
      sample = 1.0F;
    } else if (sample < -1.0F) {
      sample = -1.0F;
    }
    const int16_t pcm =
        (int16_t)(sample >= 0.0F ? (sample * 32767.0F) : (sample * 32768.0F));
    ok = write_u16_le(file, (uint16_t)pcm);
  }
  if (fclose(file) != 0) {
    ok = false;
  }
  return ok;
}
