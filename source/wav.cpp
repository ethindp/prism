// SPDX-License-Identifier: MPL-2.0

#include "wav.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <string_view>

std::size_t decode_pcm(WavSampleFormat format, std::span<const std::uint8_t> in,
                       std::span<float> out);
void encode_pcm_s16(std::span<const float> in, std::span<std::uint8_t> out);
void encode_pcm_f32(std::span<const float> in, std::span<std::uint8_t> out);

namespace {

constexpr std::uint32_t fourcc(std::string_view id) noexcept {
  return static_cast<std::uint32_t>(static_cast<unsigned char>(id[0])) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(id[1])) << 8U) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(id[2]))
          << 16U) |
         (static_cast<std::uint32_t>(static_cast<unsigned char>(id[3])) << 24U);
}

constexpr std::uint32_t RIFF_ID = fourcc("RIFF");
constexpr std::uint32_t WAVE_ID = fourcc("WAVE");
constexpr std::uint32_t FMT_ID = fourcc("fmt ");
constexpr std::uint32_t FACT_ID = fourcc("fact");
constexpr std::uint32_t DATA_ID = fourcc("data");
constexpr std::size_t RIFF_HEADER_SIZE = 12;
constexpr std::size_t CHUNK_HEADER_SIZE = 8;
constexpr std::size_t FMT_PCM_SIZE = 16;
constexpr std::size_t FMT_FLOAT_SIZE = 18;
constexpr std::size_t FMT_EXTENSIBLE_SIZE = 40;
constexpr std::size_t FACT_SIZE = 4;
constexpr std::uint16_t EXTENSIBLE_EXTRA_SIZE = 22;
// KSDATAFORMAT_SUBTYPE_PCM and _IEEE_FLOAT, less the format tag in the first
// two bytes.
constexpr std::array<std::uint8_t, 14> SUBFORMAT_SUFFIX = {
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80,
    0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
};
constexpr std::uint32_t SPEAKER_FRONT_LEFT = 0x1;
constexpr std::uint32_t SPEAKER_FRONT_RIGHT = 0x2;
constexpr std::uint32_t SPEAKER_FRONT_CENTER = 0x4;
// Integer PCM divides by its most negative value, so its range lands in
// [-1, 1); 8-bit PCM maps 0..255 onto -1..1.
constexpr float U8_SCALE = 1.0F / 127.5F;
constexpr float S16_SCALE = 1.0F / 32768.0F;
constexpr float S24_SCALE = 1.0F / 8388608.0F;
constexpr float S32_SCALE = 1.0F / 2147483648.0F;
constexpr bool SIMD_LAYOUT = std::endian::native == std::endian::little;

template <typename T> T load_le(const std::byte *p) noexcept {
  std::array<std::byte, sizeof(T)> raw{};
  std::memcpy(raw.data(), p, sizeof(T));
  if constexpr (std::endian::native == std::endian::big)
    std::ranges::reverse(raw);
  return std::bit_cast<T>(raw);
}

template <typename T> void store_le(std::byte *p, T value) noexcept {
  auto raw = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
  if constexpr (std::endian::native == std::endian::big)
    std::ranges::reverse(raw);
  std::memcpy(p, raw.data(), sizeof(T));
}

template <typename T>
T read_at(std::span<const std::byte> bytes, std::size_t offset) noexcept {
  return load_le<T>(bytes.subspan(offset, sizeof(T)).data());
}

template <typename T> void append_le(std::vector<std::byte> &out, T value) {
  std::array<std::byte, sizeof(T)> raw{};
  store_le(raw.data(), value);
  out.insert(out.end(), raw.begin(), raw.end());
}

std::int32_t load_s24(const std::byte *p) noexcept {
  const auto b0 = std::to_integer<std::uint32_t>(p[0]);
  const auto b1 = std::to_integer<std::uint32_t>(p[1]);
  const auto b2 = std::to_integer<std::uint32_t>(p[2]);
  // Flipping bit 23 offsets the sample by 2^23, so taking that back off leaves
  // it sign-extended without shifting a signed value.
  const std::uint32_t raw = b0 | (b1 << 8U) | (b2 << 16U);
  return static_cast<std::int32_t>(raw ^ 0x800000U) - 0x800000;
}

float decode_one(WavSampleFormat format, const std::byte *p) noexcept {
  switch (format) {
  case WavSampleFormat::U8:
    return (static_cast<float>(std::to_integer<std::uint8_t>(*p)) * U8_SCALE) -
           1.0F;
  case WavSampleFormat::S16:
    return static_cast<float>(load_le<std::int16_t>(p)) * S16_SCALE;
  case WavSampleFormat::S24:
    return static_cast<float>(load_s24(p)) * S24_SCALE;
  case WavSampleFormat::S32:
    return static_cast<float>(load_le<std::int32_t>(p)) * S32_SCALE;
  case WavSampleFormat::F32:
    return load_le<float>(p);
  case WavSampleFormat::F64:
    return static_cast<float>(load_le<double>(p));
  }
  return 0.0F;
}

template <typename Int> Int quantize(float sample, double scale) noexcept {
  constexpr auto lowest = static_cast<double>(std::numeric_limits<Int>::min());
  constexpr auto highest = static_cast<double>(std::numeric_limits<Int>::max());
  const double scaled = std::nearbyint(static_cast<double>(sample) * scale);
  return static_cast<Int>(std::clamp(scaled, lowest, highest));
}

void encode_one(WavSampleFormat format, float sample, std::byte *p) noexcept {
  if (std::isnan(sample))
    sample = 0.0F;
  switch (format) {
  case WavSampleFormat::U8: {
    const auto centred =
        std::nearbyint((std::clamp(sample, -1.0F, 1.0F) + 1.0F) * 127.5F);
    *p = static_cast<std::byte>(
        static_cast<std::uint8_t>(std::clamp(centred, 0.0F, 255.0F)));
    return;
  }
  case WavSampleFormat::S16:
    store_le(p, quantize<std::int16_t>(sample, 32768.0));
    return;
  case WavSampleFormat::S24: {
    const auto value = std::clamp(
        static_cast<std::int32_t>(std::nearbyint(
            static_cast<double>(std::clamp(sample, -1.0F, 1.0F)) * 8388608.0)),
        -8388608, 8388607);
    const auto bits = std::bit_cast<std::uint32_t>(value);
    p[0] = static_cast<std::byte>(bits & 0xFFU);
    p[1] = static_cast<std::byte>((bits >> 8U) & 0xFFU);
    p[2] = static_cast<std::byte>((bits >> 16U) & 0xFFU);
    return;
  }
  case WavSampleFormat::S32:
    store_le(p, quantize<std::int32_t>(sample, 2147483648.0));
    return;
  case WavSampleFormat::F32:
    store_le(p, sample);
    return;
  case WavSampleFormat::F64:
    store_le(p, static_cast<double>(sample));
    return;
  }
}

PRISM_SUPPRESS("f.6", "the SIMD kernels never throw")
void decode(const PcmView &pcm, std::span<float> out) noexcept {
  const auto format = pcm.format().sample_format();
  const auto size = bytes_per_sample(format);
  const auto *in = pcm.bytes().data();
  std::size_t done = 0;
  if constexpr (SIMD_LAYOUT) {
    if (!out.empty())
      done = decode_pcm(
          format,
          {reinterpret_cast<const std::uint8_t *>(in), pcm.bytes().size()},
          out);
  }
  for (std::size_t i = done; i < out.size(); ++i)
    out[i] = decode_one(format, in + (i * size));
}

PRISM_SUPPRESS("f.6", "the SIMD kernels never throw")
void encode(WavSampleFormat format, std::span<const float> in,
            std::span<std::byte> out) noexcept {
  if (in.empty())
    return;
  if constexpr (SIMD_LAYOUT) {
    const std::span<std::uint8_t> bytes{
        reinterpret_cast<std::uint8_t *>(out.data()), out.size()};
    if (format == WavSampleFormat::S16) {
      encode_pcm_s16(in, bytes);
      return;
    }
    if (format == WavSampleFormat::F32) {
      encode_pcm_f32(in, bytes);
      return;
    }
  }
  const auto size = bytes_per_sample(format);
  for (std::size_t i = 0; i < in.size(); ++i)
    encode_one(format, in[i], out.data() + (i * size));
}

WavResult<WavSampleFormat>
sample_format_for(std::uint16_t encoding, std::size_t container,
                  std::uint16_t valid_bits) noexcept {
  if (valid_bits == 0 || valid_bits > container * 8)
    return std::unexpected(WavError::InvalidFormat);
  if (encoding == WAV_TAG_PCM) {
    switch (container) {
    case 1:
      return WavSampleFormat::U8;
    case 2:
      return WavSampleFormat::S16;
    case 3:
      return WavSampleFormat::S24;
    case 4:
      return WavSampleFormat::S32;
    default:
      return std::unexpected(WavError::UnsupportedEncoding);
    }
  }
  if (encoding == WAV_TAG_IEEE_FLOAT) {
    if (valid_bits != container * 8)
      return std::unexpected(WavError::InvalidFormat);
    switch (container) {
    case 4:
      return WavSampleFormat::F32;
    case 8:
      return WavSampleFormat::F64;
    default:
      return std::unexpected(WavError::UnsupportedEncoding);
    }
  }
  return std::unexpected(WavError::UnsupportedEncoding);
}

WavResult<WavFormat> parse_fmt(std::span<const std::byte> body) noexcept {
  if (body.size() < FMT_PCM_SIZE)
    return std::unexpected(WavError::InvalidFormat);
  const auto tag = read_at<std::uint16_t>(body, 0);
  const auto channels = read_at<std::uint16_t>(body, 2);
  const auto sample_rate = read_at<std::uint32_t>(body, 4);
  const auto block_align = read_at<std::uint16_t>(body, 12);
  const auto bits = read_at<std::uint16_t>(body, 14);
  if (tag != WAV_TAG_EXTENSIBLE)
    return WavFormat::from_wave_format(tag, channels, sample_rate, bits,
                                       block_align);
  if (body.size() < FMT_EXTENSIBLE_SIZE ||
      read_at<std::uint16_t>(body, 16) < EXTENSIBLE_EXTRA_SIZE)
    return std::unexpected(WavError::InvalidFormat);
  auto valid_bits = read_at<std::uint16_t>(body, 18);
  const auto subformat = body.subspan(24, 16);
  if (!std::ranges::equal(std::as_bytes(std::span(SUBFORMAT_SUFFIX)),
                          subformat.subspan(2)))
    return std::unexpected(WavError::UnsupportedEncoding);
  if (channels == 0 || block_align == 0 || block_align % channels != 0)
    return std::unexpected(WavError::InvalidFormat);
  // wBitsPerSample is the container here; the valid bits are its top ones.
  const std::size_t container = block_align / channels;
  if (bits != container * 8)
    return std::unexpected(WavError::InvalidFormat);
  if (valid_bits == 0)
    valid_bits = bits;
  const auto sample_format = sample_format_for(
      read_at<std::uint16_t>(subformat, 0), container, valid_bits);
  if (!sample_format)
    return std::unexpected(sample_format.error());
  return WavFormat::make(*sample_format, channels, sample_rate);
}

WavResult<PcmView> parse_file(std::span<const std::byte> file) noexcept {
  if (file.size() < RIFF_HEADER_SIZE)
    return std::unexpected(WavError::Truncated);
  if (read_at<std::uint32_t>(file, 0) != RIFF_ID ||
      read_at<std::uint32_t>(file, 8) != WAVE_ID)
    return std::unexpected(WavError::NotRiffWave);
  // A RIFF size of 0 or one past the buffer was never filled in.
  const std::size_t riff_size = read_at<std::uint32_t>(file, 4);
  std::size_t end = file.size();
  if (riff_size >= 4 && riff_size <= file.size() - 8)
    end = riff_size + 8;
  std::optional<WavFormat> format;
  std::optional<std::span<const std::byte>> data;
  std::size_t offset = RIFF_HEADER_SIZE;
  while (end - offset >= CHUNK_HEADER_SIZE) {
    const auto id = read_at<std::uint32_t>(file, offset);
    const std::size_t size = read_at<std::uint32_t>(file, offset + 4);
    const std::size_t body = offset + CHUNK_HEADER_SIZE;
    const std::size_t available = end - body;
    if (id == DATA_ID) {
      if (data)
        return std::unexpected(WavError::DuplicateData);
      data = file.subspan(body, std::min(size, available));
    } else if (id == FMT_ID) {
      if (format)
        return std::unexpected(WavError::DuplicateFormat);
      if (size > available)
        return std::unexpected(WavError::Truncated);
      const auto parsed = parse_fmt(file.subspan(body, size));
      if (!parsed)
        return std::unexpected(parsed.error());
      format = *parsed;
    }
    // Chunks are padded to an even size; one that reaches the end is the last.
    const std::size_t padded = size + (size & 1U);
    if (padded >= available)
      break;
    offset = body + padded;
  }
  if (!format)
    return std::unexpected(WavError::MissingFormat);
  if (!data)
    return std::unexpected(WavError::MissingData);
  return PcmView::make(*format, *data);
}

// A file holding the headers for data_size bytes of audio, and room for it.
WavResult<std::vector<std::byte>> write_headers(const WavFormat &format,
                                                std::size_t data_size) {
  const bool is_float = format.sample_format() == WavSampleFormat::F32 ||
                        format.sample_format() == WavSampleFormat::F64;
  // WAVEFORMATEX cannot place more than two channels, and is ambiguous about
  // integers wider than 16 bits. Every format but integer PCM needs a fact
  // chunk.
  const bool extensible =
      format.channels() > 2 || (!is_float && format.bits_per_sample() > 16);
  std::size_t fmt_size = FMT_PCM_SIZE;
  if (extensible)
    fmt_size = FMT_EXTENSIBLE_SIZE;
  else if (is_float)
    fmt_size = FMT_FLOAT_SIZE;
  const std::size_t header = RIFF_HEADER_SIZE + CHUNK_HEADER_SIZE + fmt_size +
                             (is_float ? CHUNK_HEADER_SIZE + FACT_SIZE : 0) +
                             CHUNK_HEADER_SIZE;
  const std::size_t padding = data_size & 1U;
  constexpr std::size_t riff_max = std::numeric_limits<std::uint32_t>::max();
  if (data_size > riff_max - header - padding + 8)
    return std::unexpected(WavError::TooLarge);
  const std::uint16_t encoding = is_float ? WAV_TAG_IEEE_FLOAT : WAV_TAG_PCM;
  std::vector<std::byte> out;
  out.reserve(header + data_size + padding);
  append_le(out, RIFF_ID);
  append_le(out, static_cast<std::uint32_t>(header - 8 + data_size + padding));
  append_le(out, WAVE_ID);
  append_le(out, FMT_ID);
  append_le(out, static_cast<std::uint32_t>(fmt_size));
  append_le(out, extensible ? WAV_TAG_EXTENSIBLE : encoding);
  append_le(out, format.channels());
  append_le(out, format.sample_rate());
  append_le(out, static_cast<std::uint32_t>(format.block_align() *
                                            format.sample_rate()));
  append_le(out, static_cast<std::uint16_t>(format.block_align()));
  append_le(out, format.bits_per_sample());
  if (extensible) {
    std::uint32_t mask = 0;
    if (format.channels() == 1)
      mask = SPEAKER_FRONT_CENTER;
    else if (format.channels() == 2)
      mask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    append_le(out, EXTENSIBLE_EXTRA_SIZE);
    append_le(out, format.bits_per_sample());
    append_le(out, mask);
    append_le(out, encoding);
    const auto suffix = std::as_bytes(std::span(SUBFORMAT_SUFFIX));
    out.insert(out.end(), suffix.begin(), suffix.end());
  } else if (is_float) {
    append_le(out, std::uint16_t{0});
  }
  if (is_float) {
    append_le(out, FACT_ID);
    append_le(out, static_cast<std::uint32_t>(FACT_SIZE));
    append_le(out,
              static_cast<std::uint32_t>(data_size / format.block_align()));
  }
  append_le(out, DATA_ID);
  append_le(out, static_cast<std::uint32_t>(data_size));
  out.resize(out.size() + data_size + padding);
  return out;
}

} // namespace

WavResult<WavFormat> WavFormat::make(WavSampleFormat sample_format,
                                     std::uint16_t channels,
                                     std::uint32_t sample_rate) noexcept {
  const std::size_t size = bytes_per_sample(sample_format);
  if (size == 0 || channels == 0 || sample_rate == 0)
    return std::unexpected(WavError::InvalidFormat);
  // block_align and the byte rate are 16 and 32 bits wide in the file.
  constexpr std::size_t align_max = std::numeric_limits<std::uint16_t>::max();
  constexpr std::size_t rate_max = std::numeric_limits<std::uint32_t>::max();
  const std::size_t block_align = size * channels;
  if (block_align > align_max || block_align > rate_max / sample_rate)
    return std::unexpected(WavError::InvalidFormat);
  return WavFormat(sample_format, channels, sample_rate);
}

WavResult<WavFormat> WavFormat::from_wave_format(
    std::uint16_t format_tag, std::uint16_t channels, std::uint32_t sample_rate,
    std::uint16_t bits_per_sample, std::uint16_t block_align) noexcept {
  if (format_tag != WAV_TAG_PCM && format_tag != WAV_TAG_IEEE_FLOAT)
    return std::unexpected(WavError::UnsupportedEncoding);
  if (channels == 0 || block_align == 0 || block_align % channels != 0)
    return std::unexpected(WavError::InvalidFormat);
  const auto sample_format =
      sample_format_for(format_tag, block_align / channels, bits_per_sample);
  if (!sample_format)
    return std::unexpected(sample_format.error());
  return make(*sample_format, channels, sample_rate);
}

PcmView PcmView::make(const WavFormat &format,
                      std::span<const std::byte> bytes) noexcept {
  const std::size_t align = format.block_align();
  return {format, bytes.first(bytes.size() / align * align)};
}

std::vector<float> PcmView::to_f32() const {
  std::vector<float> out(sample_count());
  decode(*this, out);
  return out;
}

WavResult<std::span<float>>
PcmView::to_f32(std::span<float> out) const noexcept {
  if (out.size() < sample_count())
    return std::unexpected(WavError::OutputTooSmall);
  const auto written = out.first(sample_count());
  decode(*this, written);
  return written;
}

WavResult<PcmView> parse_wav(std::span<const std::byte> file) noexcept {
  return parse_file(file);
}

WavResult<PcmView> parse_wav(std::span<const std::uint8_t> file) noexcept {
  return parse_file(std::as_bytes(file));
}

WavResult<std::vector<std::byte>>
write_wav(const WavFormat &format, std::span<const float> interleaved) {
  if (interleaved.size() % format.channels() != 0)
    return std::unexpected(WavError::PartialFrame);
  const std::size_t size = bytes_per_sample(format.sample_format());
  if (interleaved.size() > std::numeric_limits<std::uint32_t>::max() / size)
    return std::unexpected(WavError::TooLarge);
  const std::size_t data_size = interleaved.size() * size;
  auto out = write_headers(format, data_size);
  if (!out)
    return out;
  encode(format.sample_format(), interleaved,
         std::span(*out).subspan(out->size() - data_size - (data_size & 1U),
                                 data_size));
  return out;
}

WavResult<std::vector<std::byte>> write_wav(const PcmView &pcm) {
  const std::size_t data_size = pcm.bytes().size();
  auto out = write_headers(pcm.format(), data_size);
  if (!out)
    return out;
  std::ranges::copy(
      pcm.bytes(),
      out->end() - static_cast<std::ptrdiff_t>(data_size + (data_size & 1U)));
  return out;
}
