// SPDX-License-Identifier: MPL-2.0

#include "wav.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "simd_kernels.cpp"
// clang-format off
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/contrib/dot/dot-inl.h"
#include "hwy/contrib/math/math-inl.h"
// clang-format on

HWY_BEFORE_NAMESPACE();
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;

HWY_ATTR void fill_frame_db_impl(std::span<const float> interleaved,
                                 std::size_t channels, std::size_t frame_len,
                                 std::size_t hop, std::size_t total_frames,
                                 std::span<float> db) {
  const hn::ScalableTag<float> d;
  const std::size_t n = db.size();
  for (std::size_t i = 0; i < n; ++i) {
    const auto start_frame = i * hop;
    const std::size_t end_frame =
        std::min(start_frame + frame_len, total_frames);
    if (end_frame <= start_frame) {
      db[i] = 0.0F;
      continue;
    }
    const auto nsamples = (end_frame - start_frame) * channels;
    const float *HWY_RESTRICT p = interleaved.data() + (start_frame * channels);
    db[i] =
        hn::Dot::Compute<0>(d, p, p, nsamples) / static_cast<float>(nsamples);
  }
  const auto N = hn::Lanes(d);
  const auto eps = hn::Set(d, 1e-16F);
  const auto scale = hn::Set(d, 10.0F);
  std::size_t i = 0;
  for (; i + N <= n; i += N) {
    const auto ms = hn::Add(hn::LoadU(d, db.data() + i), eps);
    hn::StoreU(hn::Mul(scale, hn::Log10(d, ms)), d, db.data() + i);
  }
  if (i < n) {
    const auto rest = n - i;
    const auto ms = hn::Add(hn::LoadN(d, db.data() + i, rest), eps);
    hn::StoreN(hn::Mul(scale, hn::Log10(d, ms)), d, db.data() + i, rest);
  }
}

// The PCM kernels read and write the little-endian layout of a WAV file, so
// wav.cpp only calls them on little-endian hosts. They load and store through
// byte vectors, which need no alignment.
template <typename D>
HWY_INLINE hn::VFromD<D> load_le(D d, const std::uint8_t *HWY_RESTRICT p) {
  const hn::Repartition<std::uint8_t, D> du8;
  return hn::BitCast(d, hn::LoadU(du8, p));
}

template <typename D>
HWY_INLINE hn::VFromD<D> load_le_n(D d, const std::uint8_t *HWY_RESTRICT p,
                                   std::size_t count) {
  const hn::Repartition<std::uint8_t, D> du8;
  return hn::BitCast(d, hn::LoadN(du8, p, count * sizeof(hn::TFromD<D>)));
}

template <typename D>
HWY_INLINE void store_le(hn::VFromD<D> v, D /*d*/,
                         std::uint8_t *HWY_RESTRICT p) {
  const hn::Repartition<std::uint8_t, D> du8;
  hn::StoreU(hn::BitCast(du8, v), du8, p);
}

template <typename D>
HWY_INLINE void store_le_n(hn::VFromD<D> v, D /*d*/,
                           std::uint8_t *HWY_RESTRICT p, std::size_t count) {
  const hn::Repartition<std::uint8_t, D> du8;
  hn::StoreN(hn::BitCast(du8, v), du8, p, count * sizeof(hn::TFromD<D>));
}

HWY_ATTR std::size_t decode_u8_impl(std::span<const std::uint8_t> in,
                                    std::span<float> out) {
  const hn::ScalableTag<float> d;
  const hn::Rebind<std::int32_t, decltype(d)> di;
  const hn::Rebind<std::uint8_t, decltype(d)> d8;
  const std::size_t n = out.size();
  const auto N = hn::Lanes(d);
  // 0..255 onto -1..1.
  const auto scale = hn::Set(d, 1.0F / 127.5F);
  const auto offset = hn::Set(d, -1.0F);
  std::size_t i = 0;
  for (; i + N <= n; i += N) {
    const auto v =
        hn::ConvertTo(d, hn::PromoteTo(di, hn::LoadU(d8, in.data() + i)));
    hn::StoreU(hn::MulAdd(v, scale, offset), d, out.data() + i);
  }
  if (i < n) {
    const auto rest = n - i;
    const auto v =
        hn::ConvertTo(d, hn::PromoteTo(di, hn::LoadN(d8, in.data() + i, rest)));
    hn::StoreN(hn::MulAdd(v, scale, offset), d, out.data() + i, rest);
  }
  return n;
}

HWY_ATTR std::size_t decode_s16_impl(std::span<const std::uint8_t> in,
                                     std::span<float> out) {
  const hn::ScalableTag<float> d;
  const hn::Rebind<std::int32_t, decltype(d)> di;
  const hn::Rebind<std::int16_t, decltype(d)> d16;
  const std::size_t n = out.size();
  const auto N = hn::Lanes(d);
  const auto scale = hn::Set(d, 1.0F / 32768.0F);
  std::size_t i = 0;
  for (; i + N <= n; i += N) {
    const auto v = hn::PromoteTo(di, load_le(d16, in.data() + (i * 2)));
    hn::StoreU(hn::Mul(hn::ConvertTo(d, v), scale), d, out.data() + i);
  }
  if (i < n) {
    const auto rest = n - i;
    const auto v = hn::PromoteTo(di, load_le_n(d16, in.data() + (i * 2), rest));
    hn::StoreN(hn::Mul(hn::ConvertTo(d, v), scale), d, out.data() + i, rest);
  }
  return n;
}

// Whole vectors only; the caller decodes what is left.
HWY_ATTR std::size_t decode_s24_impl(std::span<const std::uint8_t> in,
                                     std::span<float> out) {
  const hn::ScalableTag<float> d;
  const hn::Rebind<std::int32_t, decltype(d)> di;
  const hn::Rebind<std::uint32_t, decltype(d)> du;
  const hn::Rebind<std::uint8_t, decltype(d)> d8;
  const std::size_t n = out.size();
  const auto N = hn::Lanes(d);
  const auto scale = hn::Set(d, 1.0F / 8388608.0F);
  std::size_t i = 0;
  for (; i + N <= n; i += N) {
    auto b0 = hn::Zero(d8);
    auto b1 = hn::Zero(d8);
    auto b2 = hn::Zero(d8);
    hn::LoadInterleaved3(d8, in.data() + (i * 3), b0, b1, b2);
    // The sample in the top 24 bits, then shifted down to sign-extend it.
    const auto high = hn::Or(hn::Or(hn::ShiftLeft<8>(hn::PromoteTo(du, b0)),
                                    hn::ShiftLeft<16>(hn::PromoteTo(du, b1))),
                             hn::ShiftLeft<24>(hn::PromoteTo(du, b2)));
    const auto v = hn::ShiftRight<8>(hn::BitCast(di, high));
    hn::StoreU(hn::Mul(hn::ConvertTo(d, v), scale), d, out.data() + i);
  }
  return i;
}

HWY_ATTR std::size_t decode_s32_impl(std::span<const std::uint8_t> in,
                                     std::span<float> out) {
  const hn::ScalableTag<float> d;
  const hn::Rebind<std::int32_t, decltype(d)> di;
  const std::size_t n = out.size();
  const auto N = hn::Lanes(d);
  const auto scale = hn::Set(d, 1.0F / 2147483648.0F);
  std::size_t i = 0;
  for (; i + N <= n; i += N) {
    const auto v = load_le(di, in.data() + (i * 4));
    hn::StoreU(hn::Mul(hn::ConvertTo(d, v), scale), d, out.data() + i);
  }
  if (i < n) {
    const auto rest = n - i;
    const auto v = load_le_n(di, in.data() + (i * 4), rest);
    hn::StoreN(hn::Mul(hn::ConvertTo(d, v), scale), d, out.data() + i, rest);
  }
  return n;
}

HWY_ATTR std::size_t decode_f64_impl(std::span<const std::uint8_t> in,
                                     std::span<float> out) {
  const hn::ScalableTag<double> dd;
  const hn::Rebind<float, decltype(dd)> d;
  const std::size_t n = out.size();
  const auto N = hn::Lanes(dd);
  std::size_t i = 0;
  for (; i + N <= n; i += N)
    hn::StoreU(hn::DemoteTo(d, load_le(dd, in.data() + (i * 8))), d,
               out.data() + i);
  if (i < n) {
    const auto rest = n - i;
    hn::StoreN(hn::DemoteTo(d, load_le_n(dd, in.data() + (i * 8), rest)), d,
               out.data() + i, rest);
  }
  return n;
}

HWY_ATTR std::size_t decode_pcm_impl(WavSampleFormat format,
                                     std::span<const std::uint8_t> in,
                                     std::span<float> out) {
  switch (format) {
  case WavSampleFormat::U8:
    return decode_u8_impl(in, out);
  case WavSampleFormat::S16:
    return decode_s16_impl(in, out);
  case WavSampleFormat::S24:
    return decode_s24_impl(in, out);
  case WavSampleFormat::S32:
    return decode_s32_impl(in, out);
  case WavSampleFormat::F32:
    std::memcpy(out.data(), in.data(), out.size_bytes());
    return out.size();
  case WavSampleFormat::F64:
    return decode_f64_impl(in, out);
  }
  return 0;
}

template <typename V> HWY_INLINE V zero_nan(V v) {
  return hn::IfThenZeroElse(hn::IsNaN(v), v);
}

HWY_ATTR void encode_s16_impl(std::span<const float> in,
                              std::span<std::uint8_t> out) {
  const hn::ScalableTag<float> d;
  const hn::Rebind<std::int16_t, decltype(d)> d16;
  const std::size_t n = in.size();
  const auto N = hn::Lanes(d);
  const auto lo = hn::Set(d, -1.0F);
  const auto hi = hn::Set(d, 1.0F);
  const auto scale = hn::Set(d, 32768.0F);
  // DemoteTo saturates, which turns +1.0's 32768 into 32767.
  const auto to_s16 = [&](hn::VFromD<decltype(d)> v) HWY_ATTR {
    const auto clamped = hn::Clamp(zero_nan(v), lo, hi);
    return hn::DemoteTo(d16, hn::NearestInt(hn::Mul(clamped, scale)));
  };
  std::size_t i = 0;
  for (; i + N <= n; i += N)
    store_le(to_s16(hn::LoadU(d, in.data() + i)), d16, out.data() + (i * 2));
  if (i < n) {
    const auto rest = n - i;
    store_le_n(to_s16(hn::LoadN(d, in.data() + i, rest)), d16,
               out.data() + (i * 2), rest);
  }
}

HWY_ATTR void encode_f32_impl(std::span<const float> in,
                              std::span<std::uint8_t> out) {
  const hn::ScalableTag<float> d;
  const std::size_t n = in.size();
  const auto N = hn::Lanes(d);
  std::size_t i = 0;
  for (; i + N <= n; i += N)
    store_le(zero_nan(hn::LoadU(d, in.data() + i)), d, out.data() + (i * 4));
  if (i < n) {
    const auto rest = n - i;
    store_le_n(zero_nan(hn::LoadN(d, in.data() + i, rest)), d,
               out.data() + (i * 4), rest);
  }
}
} // namespace HWY_NAMESPACE
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
HWY_EXPORT(fill_frame_db_impl);
HWY_EXPORT(decode_pcm_impl);
HWY_EXPORT(encode_s16_impl);
HWY_EXPORT(encode_f32_impl);

void fill_frame_db(std::span<const float> interleaved, std::size_t channels,
                   std::size_t frame_len, std::size_t hop,
                   std::size_t total_frames, std::span<float> db) {
  HWY_DYNAMIC_DISPATCH(fill_frame_db_impl)
  (interleaved, channels, frame_len, hop, total_frames, db);
}

std::size_t decode_pcm(WavSampleFormat format, std::span<const std::uint8_t> in,
                       std::span<float> out) {
  return HWY_DYNAMIC_DISPATCH(decode_pcm_impl)(format, in, out);
}

void encode_pcm_s16(std::span<const float> in, std::span<std::uint8_t> out) {
  HWY_DYNAMIC_DISPATCH(encode_s16_impl)(in, out);
}

void encode_pcm_f32(std::span<const float> in, std::span<std::uint8_t> out) {
  HWY_DYNAMIC_DISPATCH(encode_f32_impl)(in, out);
}
#endif
