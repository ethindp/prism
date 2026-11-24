// SPDX-License-Identifier: MPL-2.0

#include "wav.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <hwy/targets.h>
#include <limits>
#include <random>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::array ALL_FORMATS = {WavSampleFormat::U8,  WavSampleFormat::S16,
                                    WavSampleFormat::S24, WavSampleFormat::S32,
                                    WavSampleFormat::F32, WavSampleFormat::F64};

// Builds WAV files byte by byte, independently of write_wav.
class FileBuilder {
public:
  FileBuilder &id(std::string_view four) {
    for (const char c : four)
      content.push_back(static_cast<std::byte>(c));
    return *this;
  }
  FileBuilder &u16(std::uint16_t v) { return le(v); }
  FileBuilder &u32(std::uint32_t v) { return le(v); }
  FileBuilder &raw(std::span<const std::byte> data) {
    content.insert(content.end(), data.begin(), data.end());
    return *this;
  }
  FileBuilder &chunk(std::string_view four, std::span<const std::byte> body) {
    id(four).u32(static_cast<std::uint32_t>(body.size())).raw(body);
    if (body.size() % 2 != 0)
      content.push_back(std::byte{0});
    return *this;
  }
  [[nodiscard]] std::vector<std::byte> riff() const {
    FileBuilder out;
    out.id("RIFF").u32(static_cast<std::uint32_t>(content.size() + 4));
    out.id("WAVE").raw(content);
    return out.content;
  }
  [[nodiscard]] const std::vector<std::byte> &bytes() const { return content; }

private:
  template <typename T> FileBuilder &le(T v) {
    for (std::size_t i = 0; i < sizeof(T); ++i)
      content.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFFU));
    return *this;
  }

  std::vector<std::byte> content;
};

std::vector<std::byte> fmt_body(std::uint16_t tag, std::uint16_t channels,
                                std::uint32_t rate, std::uint16_t block_align,
                                std::uint16_t bits) {
  FileBuilder b;
  b.u16(tag).u16(channels).u32(rate).u32(rate * block_align);
  b.u16(block_align).u16(bits);
  return b.bytes();
}

std::vector<std::byte> extensible_body(std::uint16_t subformat,
                                       std::uint16_t channels,
                                       std::uint32_t rate,
                                       std::uint16_t container_bits,
                                       std::uint16_t valid_bits) {
  const auto block_align =
      static_cast<std::uint16_t>(container_bits / 8 * channels);
  FileBuilder b;
  b.raw(fmt_body(WAV_TAG_EXTENSIBLE, channels, rate, block_align,
                 container_bits));
  b.u16(22).u16(valid_bits).u32(0).u16(subformat).u16(0).u16(0).u16(0x0010);
  constexpr std::array<std::uint8_t, 8> tail = {0x80, 0x00, 0x00, 0xAA,
                                                0x00, 0x38, 0x9B, 0x71};
  b.raw(std::as_bytes(std::span(tail)));
  return b.bytes();
}

std::vector<std::byte> random_bytes(std::size_t n, std::uint32_t seed) {
  std::mt19937 rng(seed);
  std::uniform_int_distribution<int> dist(0, 255);
  std::vector<std::byte> out(n);
  for (auto &b : out)
    b = static_cast<std::byte>(dist(rng));
  return out;
}

// What each encoding means, written out the obvious way.
float reference_sample(WavSampleFormat format, const std::byte *p) {
  const auto u = [&](std::size_t i) {
    return std::to_integer<std::uint32_t>(p[i]);
  };
  switch (format) {
  case WavSampleFormat::U8:
    return (static_cast<float>(u(0)) / 127.5F) - 1.0F;
  case WavSampleFormat::S16:
    return static_cast<float>(static_cast<std::int16_t>(u(0) | (u(1) << 8))) /
           32768.0F;
  case WavSampleFormat::S24: {
    auto v = static_cast<std::int32_t>(u(0) | (u(1) << 8) | (u(2) << 16));
    if (v >= 0x800000)
      v -= 0x1000000;
    return static_cast<float>(v) / 8388608.0F;
  }
  case WavSampleFormat::S32:
    return static_cast<float>(
        static_cast<double>(static_cast<std::int32_t>(
            u(0) | (u(1) << 8) | (u(2) << 16) | (u(3) << 24))) /
        2147483648.0);
  case WavSampleFormat::F32: {
    float f = 0;
    std::memcpy(&f, p, 4);
    return f;
  }
  case WavSampleFormat::F64: {
    double d = 0;
    std::memcpy(&d, p, 8);
    return static_cast<float>(d);
  }
  }
  return 0;
}

// Random bytes are fine for integer formats, but most random bit patterns are
// NaN or huge as floats, so float formats get real samples.
std::vector<std::byte> sample_bytes(WavSampleFormat format, std::size_t samples,
                                    std::uint32_t seed) {
  const std::size_t size = bytes_per_sample(format);
  if (format != WavSampleFormat::F32 && format != WavSampleFormat::F64)
    return random_bytes(samples * size, seed);
  std::mt19937 rng(seed);
  std::uniform_real_distribution<double> dist(-1.5, 1.5);
  std::vector<std::byte> out(samples * size);
  for (std::size_t i = 0; i < samples; ++i) {
    const double v = dist(rng);
    if (format == WavSampleFormat::F32) {
      const auto f = static_cast<float>(v);
      std::memcpy(out.data() + (i * 4), &f, 4);
    } else {
      std::memcpy(out.data() + (i * 8), &v, 8);
    }
  }
  return out;
}

// Runs body once for every SIMD target this machine can run.
template <typename F> void for_each_target(F &&body) {
  for (const auto target : hwy::SupportedAndGeneratedTargets()) {
    hwy::SetSupportedTargetsForTest(target);
    SCOPED_TRACE(hwy::TargetName(target));
    body();
  }
  hwy::SetSupportedTargetsForTest(0);
}

WavFormat make_format(WavSampleFormat f, std::uint16_t channels,
                      std::uint32_t rate = 48000) {
  auto format = WavFormat::make(f, channels, rate);
  EXPECT_TRUE(format.has_value());
  return *format;
}

} // namespace

TEST(Wav, DecodesEveryFormatOnEveryTarget) {
  for_each_target([] {
    for (const auto f : ALL_FORMATS) {
      const std::size_t size = bytes_per_sample(f);
      // Every length up to several vectors of the widest target, so that
      // whole-vector loops and every tail length run.
      for (std::size_t n = 0; n <= 70; ++n) {
        SCOPED_TRACE(n);
        // Offset by one byte, so loads are never aligned to the sample.
        std::vector<std::byte> storage(1);
        const auto samples = sample_bytes(f, n, static_cast<std::uint32_t>(n));
        storage.insert(storage.end(), samples.begin(), samples.end());
        const auto bytes = std::span(storage).subspan(1, n * size);
        const auto pcm = PcmView::make(make_format(f, 1), bytes);
        const auto decoded = pcm.to_f32();
        ASSERT_EQ(decoded.size(), n);
        for (std::size_t i = 0; i < n; ++i)
          ASSERT_NEAR(decoded[i], reference_sample(f, &bytes[i * size]), 1e-6F)
              << "sample " << i;
      }
    }
  });
}

TEST(Wav, DecodesExtremes) {
  const std::array<std::byte, 4> s16 = {std::byte{0x00}, std::byte{0x80},
                                        std::byte{0xFF}, std::byte{0x7F}};
  const auto v =
      PcmView::make(make_format(WavSampleFormat::S16, 1), s16).to_f32();
  EXPECT_EQ(v[0], -1.0F);
  EXPECT_NEAR(v[1], 32767.0F / 32768.0F, 1e-7F);

  const std::array<std::byte, 2> u8 = {std::byte{0x00}, std::byte{0xFF}};
  const auto w =
      PcmView::make(make_format(WavSampleFormat::U8, 1), u8).to_f32();
  EXPECT_NEAR(w[0], -1.0F, 1e-6F);
  EXPECT_NEAR(w[1], 1.0F, 1e-6F);
}

TEST(Wav, ParsesCanonicalPcm) {
  const auto samples = random_bytes(400, 1);
  FileBuilder b;
  b.chunk("fmt ", fmt_body(1, 2, 22050, 4, 16)).chunk("data", samples);
  const auto file = b.riff();
  ASSERT_EQ(file.size(), 44 + samples.size());
  const auto pcm = parse_wav(file);
  ASSERT_TRUE(pcm) << std::to_underlying(pcm.error());
  EXPECT_EQ(pcm->format().sample_format(), WavSampleFormat::S16);
  EXPECT_EQ(pcm->format().channels(), 2);
  EXPECT_EQ(pcm->format().sample_rate(), 22050U);
  EXPECT_EQ(pcm->format().bits_per_sample(), 16);
  EXPECT_EQ(pcm->frame_count(), 100U);
  EXPECT_EQ(pcm->sample_count(), 200U);
  EXPECT_EQ(pcm->bytes().data(), file.data() + 44);
}

TEST(Wav, ParsesAcrossOtherChunksAndPadding) {
  // An odd-sized chunk before fmt, padding, and a chunk after data.
  const std::array<std::byte, 3> odd = {std::byte{1}, std::byte{2},
                                        std::byte{3}};
  const auto samples = random_bytes(6, 2);
  FileBuilder b;
  b.chunk("LIST", odd).chunk("fmt ", fmt_body(1, 1, 8000, 2, 16));
  b.chunk("data", samples).chunk("junk", odd);
  const auto pcm = parse_wav(b.riff());
  ASSERT_TRUE(pcm) << std::to_underlying(pcm.error());
  EXPECT_EQ(pcm->sample_count(), 3U);
}

TEST(Wav, ParsesDataBeforeFormat) {
  FileBuilder b;
  b.chunk("data", random_bytes(4, 3))
      .chunk("fmt ", fmt_body(1, 1, 8000, 2, 16));
  const auto pcm = parse_wav(b.riff());
  ASSERT_TRUE(pcm);
  EXPECT_EQ(pcm->sample_count(), 2U);
}

TEST(Wav, ToleratesSizesAStreamingWriterLeaves) {
  // RIFF and data sizes of 0xFFFFFFFF: everything up to the end is audio.
  FileBuilder b;
  b.chunk("fmt ", fmt_body(1, 1, 8000, 2, 16));
  b.id("data").u32(0xFFFFFFFFU).raw(random_bytes(9, 4));
  auto file = b.riff();
  for (std::size_t i = 4; i < 8; ++i)
    file[i] = std::byte{0xFF};
  const auto pcm = parse_wav(file);
  ASSERT_TRUE(pcm);
  // 9 bytes hold 4 whole 16-bit samples; the odd byte is left out.
  EXPECT_EQ(pcm->sample_count(), 4U);

  for (std::size_t i = 4; i < 8; ++i)
    file[i] = std::byte{0};
  ASSERT_TRUE(parse_wav(file));
}

TEST(Wav, IgnoresBytesPastTheRiffSize) {
  FileBuilder b;
  b.chunk("fmt ", fmt_body(1, 1, 8000, 2, 16))
      .chunk("data", random_bytes(4, 5));
  auto file = b.riff();
  const auto junk = random_bytes(7, 6);
  file.insert(file.end(), junk.begin(), junk.end());
  const auto pcm = parse_wav(file);
  ASSERT_TRUE(pcm);
  EXPECT_EQ(pcm->sample_count(), 2U);
}

TEST(Wav, ParsesExtensible) {
  struct Case {
    std::uint16_t subformat;
    std::uint16_t container;
    std::uint16_t valid;
    WavSampleFormat expected;
  };
  for (const auto &c : {Case{1, 24, 24, WavSampleFormat::S24},
                        Case{1, 32, 24, WavSampleFormat::S32},
                        Case{1, 16, 12, WavSampleFormat::S16},
                        Case{3, 32, 32, WavSampleFormat::F32},
                        Case{3, 64, 0, WavSampleFormat::F64}}) {
    FileBuilder b;
    b.chunk("fmt ",
            extensible_body(c.subformat, 6, 48000, c.container, c.valid));
    b.chunk("data", random_bytes(c.container / 8 * 6 * 2, 7));
    const auto pcm = parse_wav(b.riff());
    ASSERT_TRUE(pcm) << std::to_underlying(pcm.error());
    EXPECT_EQ(pcm->format().sample_format(), c.expected);
    EXPECT_EQ(pcm->format().channels(), 6);
    EXPECT_EQ(pcm->frame_count(), 2U);
  }
}

TEST(Wav, RejectsMalformedFiles) {
  const auto data = random_bytes(4, 8);
  const auto with_fmt = [&](const std::vector<std::byte> &fmt) {
    FileBuilder b;
    b.chunk("fmt ", fmt).chunk("data", data);
    return parse_wav(b.riff());
  };
  const auto error_of = [](const auto &result) {
    EXPECT_FALSE(result.has_value());
    return result ? WavError::TooLarge : result.error();
  };

  EXPECT_EQ(error_of(parse_wav(std::span<const std::byte>{})),
            WavError::Truncated);
  {
    FileBuilder b;
    b.chunk("fmt ", fmt_body(1, 1, 8000, 2, 16)).chunk("data", data);
    auto rifx = b.riff();
    rifx[3] = std::byte{'X'};
    EXPECT_EQ(error_of(parse_wav(rifx)), WavError::NotRiffWave);
  }
  {
    FileBuilder b;
    b.chunk("data", data);
    EXPECT_EQ(error_of(parse_wav(b.riff())), WavError::MissingFormat);
  }
  {
    FileBuilder b;
    b.chunk("fmt ", fmt_body(1, 1, 8000, 2, 16));
    EXPECT_EQ(error_of(parse_wav(b.riff())), WavError::MissingData);
  }
  {
    FileBuilder b;
    b.chunk("fmt ", fmt_body(1, 1, 8000, 2, 16)).chunk("data", data);
    b.chunk("data", data);
    EXPECT_EQ(error_of(parse_wav(b.riff())), WavError::DuplicateData);
  }
  {
    FileBuilder b;
    b.chunk("fmt ", fmt_body(1, 1, 8000, 2, 16));
    b.chunk("fmt ", fmt_body(1, 1, 8000, 2, 16)).chunk("data", data);
    EXPECT_EQ(error_of(parse_wav(b.riff())), WavError::DuplicateFormat);
  }
  {
    FileBuilder b;
    b.id("fmt ").u32(16).u16(1);
    auto file = b.riff();
    EXPECT_EQ(error_of(parse_wav(file)), WavError::Truncated);
  }
  // ADPCM.
  EXPECT_EQ(error_of(with_fmt(fmt_body(2, 1, 8000, 256, 4))),
            WavError::UnsupportedEncoding);
  // Zero channels, a block align that is not a whole sample per channel, and
  // more bits than the container holds.
  EXPECT_EQ(error_of(with_fmt(fmt_body(1, 0, 8000, 2, 16))),
            WavError::InvalidFormat);
  EXPECT_EQ(error_of(with_fmt(fmt_body(1, 2, 8000, 3, 16))),
            WavError::InvalidFormat);
  EXPECT_EQ(error_of(with_fmt(fmt_body(1, 1, 8000, 2, 24))),
            WavError::InvalidFormat);
  EXPECT_EQ(error_of(with_fmt(fmt_body(1, 1, 0, 2, 16))),
            WavError::InvalidFormat);
  // A 16-bit float, and a float narrower than its container.
  EXPECT_EQ(error_of(with_fmt(fmt_body(3, 1, 8000, 2, 16))),
            WavError::UnsupportedEncoding);
  EXPECT_EQ(error_of(with_fmt(fmt_body(3, 1, 8000, 4, 24))),
            WavError::InvalidFormat);
  // A short fmt chunk.
  EXPECT_EQ(error_of(with_fmt(random_bytes(14, 9))), WavError::InvalidFormat);
  // An extensible fmt with an unknown subformat GUID.
  {
    auto body = extensible_body(1, 2, 8000, 16, 16);
    body.back() = std::byte{0};
    EXPECT_EQ(error_of(with_fmt(body)), WavError::UnsupportedEncoding);
  }
}

TEST(Wav, SurvivesCorruptInput) {
  FileBuilder b;
  b.chunk("LIST", random_bytes(5, 10));
  b.chunk("fmt ", extensible_body(1, 2, 44100, 24, 24));
  b.chunk("data", random_bytes(600, 11));
  const auto valid = b.riff();
  std::mt19937 rng(12);
  std::uniform_int_distribution<std::size_t> pos(0, valid.size() - 1);
  std::uniform_int_distribution<int> byte(0, 255);
  for (int round = 0; round < 5000; ++round) {
    auto file = valid;
    for (int flips = 1 + (round % 4); flips > 0; --flips)
      file[pos(rng)] = static_cast<std::byte>(byte(rng));
    file.resize(pos(rng) + 1);
    if (const auto pcm = parse_wav(file)) {
      // Whatever parsed must lie inside the file and decode without fault.
      ASSERT_GE(pcm->bytes().data(), file.data());
      ASSERT_LE(pcm->bytes().data() + pcm->bytes().size(),
                file.data() + file.size());
      (void)pcm->to_f32();
    }
  }
}

TEST(Wav, PcmViewKeepsWholeFrames) {
  const auto bytes = random_bytes(13, 13);
  const auto pcm = PcmView::make(make_format(WavSampleFormat::S16, 2), bytes);
  EXPECT_EQ(pcm.bytes().size(), 12U);
  EXPECT_EQ(pcm.frame_count(), 3U);
  EXPECT_EQ(pcm.sample_count(), 6U);
}

TEST(Wav, DecodesIntoACallerBuffer) {
  const auto bytes = random_bytes(8, 14);
  const auto pcm = PcmView::make(make_format(WavSampleFormat::S16, 1), bytes);
  std::array<float, 3> small{};
  const auto too_small = pcm.to_f32(small);
  ASSERT_FALSE(too_small);
  EXPECT_EQ(too_small.error(), WavError::OutputTooSmall);
  std::array<float, 6> big{};
  big.fill(42.0F);
  const auto written = pcm.to_f32(big);
  ASSERT_TRUE(written);
  EXPECT_EQ(written->size(), 4U);
  EXPECT_EQ(written->data(), big.data());
  EXPECT_EQ(big[4], 42.0F);
}

TEST(Wav, ValidatesFormats) {
  EXPECT_FALSE(WavFormat::make(WavSampleFormat::S16, 0, 48000));
  EXPECT_FALSE(WavFormat::make(WavSampleFormat::S16, 2, 0));
  EXPECT_FALSE(WavFormat::make(static_cast<WavSampleFormat>(200), 1, 48000));
  // block_align is a 16-bit field.
  EXPECT_FALSE(WavFormat::make(WavSampleFormat::F64, 8192, 48000));
  EXPECT_TRUE(WavFormat::make(WavSampleFormat::F64, 8191, 48000));
  const auto pcm = WavFormat::from_wave_format(1, 1, 16000, 16, 2);
  ASSERT_TRUE(pcm);
  EXPECT_EQ(pcm->sample_format(), WavSampleFormat::S16);
  EXPECT_EQ(WavFormat::from_wave_format(2, 1, 16000, 4, 256).error(),
            WavError::UnsupportedEncoding);
}

TEST(Wav, RoundTripsEveryFormat) {
  for_each_target([] {
    for (const auto f : ALL_FORMATS) {
      for (const std::uint16_t channels :
           std::array<std::uint16_t, 3>{1, 2, 6}) {
        SCOPED_TRACE(static_cast<int>(f));
        SCOPED_TRACE(channels);
        const auto format = make_format(f, channels, 44100);
        std::mt19937 rng(15);
        std::uniform_real_distribution<float> dist(-1.0F, 1.0F);
        std::vector<float> in(static_cast<std::size_t>(channels) * 37);
        for (auto &s : in)
          s = dist(rng);
        const auto file = write_wav(format, in);
        ASSERT_TRUE(file);
        const auto pcm = parse_wav(*file);
        ASSERT_TRUE(pcm) << std::to_underlying(pcm.error());
        EXPECT_EQ(pcm->format(), format);
        const auto out = pcm->to_f32();
        ASSERT_EQ(out.size(), in.size());
        const float step =
            f == WavSampleFormat::U8 ? 1.0F / 127.5F : 1.0F / 32768.0F;
        for (std::size_t i = 0; i < in.size(); ++i)
          ASSERT_NEAR(out[i], in[i], step) << "sample " << i;

        // Wrapping the decoded bytes again gives the same file.
        const auto again = write_wav(*pcm);
        ASSERT_TRUE(again);
        EXPECT_EQ(*again, *file);
      }
    }
  });
}

TEST(Wav, WritesTheExpectedHeaders) {
  const std::array<float, 2> two = {0.0F, 0.0F};
  const auto pcm16 = write_wav(make_format(WavSampleFormat::S16, 2), two);
  ASSERT_TRUE(pcm16);
  EXPECT_EQ(pcm16->size(), 44U + 4U);

  const auto float32 = write_wav(make_format(WavSampleFormat::F32, 1), two);
  ASSERT_TRUE(float32);
  // fmt with cbSize (18 bytes) and a fact chunk.
  EXPECT_EQ(float32->size(), 12U + 8U + 18U + 12U + 8U + 8U);

  const auto s24 = write_wav(make_format(WavSampleFormat::S24, 1), two);
  ASSERT_TRUE(s24);
  // 24-bit integers are written as WAVE_FORMAT_EXTENSIBLE.
  EXPECT_EQ(std::to_integer<int>((*s24)[20]), 0xFE);
  // A single 3-byte sample needs a pad byte after it.
  const std::array<float, 1> one = {0.0F};
  const auto odd = write_wav(make_format(WavSampleFormat::S24, 1), one);
  ASSERT_TRUE(odd);
  EXPECT_EQ(odd->size() % 2, 0U);
  EXPECT_TRUE(parse_wav(*odd));
}

TEST(Wav, SaturatesAndSilencesNan) {
  for_each_target([] {
    const std::array<float, 6> in = {2.0F, -2.0F, std::nanf(""),
                                     1.0F, -1.0F, 0.5F};
    const auto file = write_wav(make_format(WavSampleFormat::S16, 1), in);
    ASSERT_TRUE(file);
    const auto pcm = parse_wav(*file);
    ASSERT_TRUE(pcm);
    std::array<std::int16_t, 6> samples{};
    std::memcpy(samples.data(), pcm->bytes().data(), sizeof(samples));
    EXPECT_EQ(samples, (std::array<std::int16_t, 6>{32767, -32768, 0, 32767,
                                                    -32768, 16384}));

    const auto f32 = write_wav(make_format(WavSampleFormat::F32, 1), in);
    ASSERT_TRUE(f32);
    const auto floats = parse_wav(*f32)->to_f32();
    EXPECT_EQ(floats[0], 2.0F);
    EXPECT_EQ(floats[2], 0.0F);
  });
}

TEST(Wav, RejectsPartialFrames) {
  const std::array<float, 3> three{};
  const auto file = write_wav(make_format(WavSampleFormat::S16, 2), three);
  ASSERT_FALSE(file);
  EXPECT_EQ(file.error(), WavError::PartialFrame);
}
