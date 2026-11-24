// SPDX-License-Identifier: MPL-2.0

#pragma once
#include "annotations.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

// Uncompressed RIFF/WAVE audio held in memory. A WavFormat is validated when it
// is made and a PcmView only ever holds whole frames, so code that has either
// never checks them again.

enum class WavSampleFormat : std::uint8_t {
  U8, // unsigned, 128 is silence
  S16,
  S24, // packed into three bytes
  S32,
  F32,
  F64,
};

enum class WavError : std::uint8_t {
  Truncated,
  NotRiffWave,
  MissingFormat,
  DuplicateFormat,
  MissingData,
  DuplicateData,
  UnsupportedEncoding,
  InvalidFormat,
  OutputTooSmall,
  PartialFrame,
  TooLarge,
};

template <typename T = void> using WavResult = std::expected<T, WavError>;

// WAVEFORMATEX::wFormatTag values. mmreg.h's names for them are macros.
inline constexpr std::uint16_t WAV_TAG_PCM = 0x0001;
inline constexpr std::uint16_t WAV_TAG_IEEE_FLOAT = 0x0003;
inline constexpr std::uint16_t WAV_TAG_EXTENSIBLE = 0xFFFE;

[[nodiscard]] constexpr std::size_t
bytes_per_sample(WavSampleFormat format) noexcept {
  switch (format) {
  case WavSampleFormat::U8:
    return 1;
  case WavSampleFormat::S16:
    return 2;
  case WavSampleFormat::S24:
    return 3;
  case WavSampleFormat::S32:
  case WavSampleFormat::F32:
    return 4;
  case WavSampleFormat::F64:
    return 8;
  }
  return 0;
}

class WavFormat {
public:
  [[nodiscard]] static WavResult<WavFormat>
  make(WavSampleFormat sample_format, std::uint16_t channels,
       std::uint32_t sample_rate) noexcept;
  // From WAVEFORMATEX fields. The container size is block_align / channels;
  // bits_per_sample may be smaller (12 bits in 16), since the valid bits are
  // the high ones.
  [[nodiscard]] static WavResult<WavFormat>
  from_wave_format(std::uint16_t format_tag, std::uint16_t channels,
                   std::uint32_t sample_rate, std::uint16_t bits_per_sample,
                   std::uint16_t block_align) noexcept;
  [[nodiscard]] constexpr WavSampleFormat sample_format() const noexcept {
    return kind;
  }
  [[nodiscard]] constexpr std::uint16_t channels() const noexcept {
    return channel_count;
  }
  [[nodiscard]] constexpr std::uint32_t sample_rate() const noexcept {
    return rate;
  }
  [[nodiscard]] constexpr std::uint16_t bits_per_sample() const noexcept {
    return static_cast<std::uint16_t>(bytes_per_sample(kind) * 8);
  }
  [[nodiscard]] constexpr std::size_t block_align() const noexcept {
    return bytes_per_sample(kind) * channel_count;
  }
  friend constexpr bool operator==(const WavFormat &,
                                   const WavFormat &) = default;

private:
  constexpr WavFormat(WavSampleFormat sample_format, std::uint16_t channels,
                      std::uint32_t sample_rate) noexcept
      : kind(sample_format), channel_count(channels), rate(sample_rate) {}
  WavSampleFormat kind;
  std::uint16_t channel_count;
  std::uint32_t rate;
};

// Interleaved samples borrowed from a buffer the caller keeps alive. The span
// parameters are not PRISM_LIFETIMEBOUND: MSVC ties that to the span object
// itself, which makes every call passing a vector look dangling.
class PRISM_GSL_POINTER(std::byte) PcmView {
public:
  // A partial frame at the end, as a cut-off capture leaves, is left out.
  [[nodiscard]] static PcmView make(const WavFormat &format,
                                    std::span<const std::byte> bytes) noexcept;
  [[nodiscard]] constexpr const WavFormat &format() const noexcept {
    return layout;
  }
  [[nodiscard]] constexpr std::span<const std::byte> bytes() const noexcept {
    return data;
  }
  [[nodiscard]] constexpr std::size_t frame_count() const noexcept {
    return data.size() / layout.block_align();
  }
  [[nodiscard]] constexpr std::size_t sample_count() const noexcept {
    return data.size() / bytes_per_sample(layout.sample_format());
  }
  // Integer samples land in [-1, 1); float samples pass through unchanged.
  [[nodiscard]] std::vector<float> to_f32() const;
  // Writes sample_count() floats to the front of out and returns them.
  [[nodiscard]] WavResult<std::span<float>>
  to_f32(std::span<float> out) const noexcept;

private:
  constexpr PcmView(const WavFormat &format,
                    std::span<const std::byte> bytes) noexcept
      : layout(format), data(bytes) {}
  WavFormat layout;
  std::span<const std::byte> data;
};

// The view borrows the data chunk from file. Sizes that run past the end of
// file, which a streaming writer leaves when it cannot go back to fill them
// in, are cut to what is there.
[[nodiscard]] WavResult<PcmView>
parse_wav(std::span<const std::byte> file) noexcept;
[[nodiscard]] WavResult<PcmView>
parse_wav(std::span<const std::uint8_t> file) noexcept;

// Rounds to the nearest integer sample, saturates outside [-1, 1] and writes
// NaN as silence. interleaved must hold whole frames.
[[nodiscard]] WavResult<std::vector<std::byte>>
write_wav(const WavFormat &format, std::span<const float> interleaved);
[[nodiscard]] WavResult<std::vector<std::byte>> write_wav(const PcmView &pcm);
