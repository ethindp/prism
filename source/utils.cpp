/* NVGT - NonVisual Gaming Toolkit
 * Copyright (c) 2022-2025 Sam Tupy
 * https://nvgt.dev
 * This software is provided "as-is", without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from the
 * use of this software. Permission is granted to anyone to use this software
 * for any purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 * 1. The origin of this software must not be misrepresented; you must not claim
 * that you wrote the original software. If you use this software in a product,
 * an acknowledgment in the product documentation would be appreciated but is
 * not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 * misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "utils.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>
#include <utility>

// Begin NVGT code
double range_convert(double v, double a0, double a1, double b0, double b1) {
  const double t = (v - a0) / (a1 - a0);
  return std::lerp(b0, b1, t);
}

float range_convert(float old_value, float old_min, float old_max,
                    float new_min, float new_max) {
  return (((old_value - old_min) / (old_max - old_min)) * (new_max - new_min)) +
         new_min;
}

float range_convert_midpoint(float old_value, float old_min, float old_midpoint,
                             float old_max, float new_min, float new_midpoint,
                             float new_max) {
  if (old_value <= old_midpoint)
    return range_convert(old_value, old_min, old_midpoint, new_min,
                         new_midpoint);
  else
    return range_convert(old_value, old_midpoint, old_max, new_midpoint,
                         new_max);
}
// End NVGT code

double exp_range_convert(float t, double out_min, double out_mid,
                         double out_max) {
  const double log_min = std::log(out_min);
  const double log_mid = std::log(out_mid);
  const double log_max = std::log(out_max);
  double log_val;
  if (t <= 0.5)
    log_val = log_min + ((log_mid - log_min) * (t / 0.5));
  else
    log_val = log_mid + ((log_max - log_mid) * ((t - 0.5) / 0.5));
  return std::exp(log_val);
}

float exp_range_convert_inv(double val, double out_min, double out_mid,
                            double out_max) {
  const double log_min = std::log(out_min);
  const double log_mid = std::log(out_mid);
  const double log_max = std::log(out_max);
  double log_val = std::log(std::clamp(val, out_min, out_max));
  if (log_val <= log_mid)
    return static_cast<float>(0.5 * (log_val - log_min) / (log_mid - log_min));
  else
    return static_cast<float>(
        0.5 + ((0.5 * (log_val - log_mid)) / (log_max - log_mid)));
}

struct TrimBounds {
  std::size_t start_frame = 0;
  std::size_t end_frame = 0;
  bool speech_detected = false;
  float noise_floor_db = -160.0F;
  float open_thr_db = -160.0F;
  float close_thr_db = -160.0F;
};

struct TrimWorkspace {
  std::vector<float> db;
  std::vector<float> scratch;
};

[[nodiscard]] static inline std::span<const float>
hann_window(std::size_t fade_frames) {
  static thread_local std::vector<float> cache;
  if (cache.size() != fade_frames) {
    cache.resize(fade_frames);
    const auto denom = static_cast<double>(fade_frames - 1);
    for (std::size_t i = 0; i < fade_frames; ++i) {
      const double t = static_cast<double>(i) / denom;
      cache[i] =
          static_cast<float>(0.5 - (0.5 * std::cos(std::numbers::pi * t)));
    }
  }
  return {cache.data(), cache.size()};
}

static inline std::size_t ms_to_frames(float ms, std::size_t sample_rate) {
  const auto f =
      (static_cast<double>(ms) * static_cast<double>(sample_rate)) / 1000.0;
  return static_cast<std::size_t>(std::max(0.0, std::floor(f + 0.5)));
}

static inline float mean_square_to_db(double mean_square) {
  constexpr double eps = 1e-16;
  return static_cast<float>(10.0 * std::log10(mean_square + eps));
}

[[gnu::hot]]
static inline float frame_db(std::span<const float> interleaved,
                             std::size_t start_frame, std::size_t frame_len,
                             std::size_t total_frames, std::size_t channels) {
  const auto end_frame = std::min(start_frame + frame_len, total_frames);
  if (end_frame <= start_frame)
    return -160.0F;
  const std::size_t nframes = end_frame - start_frame;
  const std::size_t nsamples = nframes * channels;
  const float *__restrict p = interleaved.data() + (start_frame * channels);
  double s0 = 0;
  double s1 = 0;
  double s2 = 0;
  double s3 = 0;
  double s4 = 0;
  double s5 = 0;
  double s6 = 0;
  double s7 = 0;
  std::size_t i = 0;
  const std::size_t lim = nsamples & ~static_cast<std::size_t>(7);
  for (; i < lim; i += 8) {
    const double v0 = p[i + 0];
    const double v1 = p[i + 1];
    const double v2 = p[i + 2];
    const double v3 = p[i + 3];
    const double v4 = p[i + 4];
    const double v5 = p[i + 5];
    const double v6 = p[i + 6];
    const double v7 = p[i + 7];
    s0 += v0 * v0;
    s1 += v1 * v1;
    s2 += v2 * v2;
    s3 += v3 * v3;
    s4 += v4 * v4;
    s5 += v5 * v5;
    s6 += v6 * v6;
    s7 += v7 * v7;
  }
  // Pairwise tree reduction
  double sumsq = ((s0 + s1) + (s2 + s3)) + ((s4 + s5) + (s6 + s7));
  for (; i < nsamples; ++i) {
    const double v = p[i];
    sumsq += v * v;
  }
  return mean_square_to_db(sumsq / static_cast<double>(nsamples));
}

static inline float percentile(std::span<const float> x, float p,
                               std::vector<float> &scratch) {
  if (x.empty())
    return -160.0F;
  p = std::clamp(p, 0.0F, 1.0F);
  scratch.assign(x.begin(), x.end()); // reuses capacity
  const std::size_t n = scratch.size();
  if (n == 1)
    return scratch[0];
  const auto k = static_cast<std::size_t>(
      std::floor(static_cast<double>(p) * static_cast<double>(n - 1)));
  std::nth_element(scratch.begin(),
                   scratch.begin() + static_cast<std::ptrdiff_t>(k),
                   scratch.end());
  return scratch[k];
}

static inline void apply_fade_in(std::span<float> interleaved,
                                 std::size_t channels,
                                 std::size_t fade_frames) {
  if (fade_frames == 0 || channels == 0)
    return;
  const auto total_frames = interleaved.size() / channels;
  fade_frames = std::min(fade_frames, total_frames);
  if (fade_frames <= 1)
    return;
  const auto window = hann_window(fade_frames);
  const float *__restrict w = window.data();
  if (channels == 1) {
    float *__restrict p = interleaved.data();
    for (std::size_t i = 0; i < fade_frames; ++i)
      p[i] *= w[i];
    return;
  }
  if (channels == 2) {
    float *__restrict p = interleaved.data();
    for (std::size_t i = 0; i < fade_frames; ++i) {
      const float g = w[i];
      p[(2 * i) + 0] *= g;
      p[(2 * i) + 1] *= g;
    }
    return;
  }
  for (std::size_t i = 0; i < fade_frames; ++i) {
    const float g = w[i];
    const auto base = i * channels;
    for (std::size_t ch = 0; ch < channels; ++ch)
      interleaved[base + ch] *= g;
  }
}

static inline void apply_fade_out(std::span<float> interleaved,
                                  std::size_t channels,
                                  std::size_t fade_frames) {
  if (fade_frames == 0 || channels == 0)
    return;
  const auto total_frames = interleaved.size() / channels;
  fade_frames = std::min(fade_frames, total_frames);
  if (fade_frames == 0)
    return;
  const auto start = total_frames - fade_frames;
  if (fade_frames == 1) {
    const auto base = start * channels;
    for (std::size_t ch = 0; ch < channels; ++ch)
      interleaved[base + ch] = 0.0F;
    return;
  }
  const auto window = hann_window(fade_frames);
  const float *__restrict w = window.data();
  const std::size_t last = fade_frames - 1;
  if (channels == 1) {
    float *__restrict p = interleaved.data() + start;
    for (std::size_t i = 0; i < fade_frames; ++i)
      p[i] *= w[last - i];
    return;
  }
  if (channels == 2) {
    float *__restrict p = interleaved.data() + (start * 2);
    for (std::size_t i = 0; i < fade_frames; ++i) {
      const float g = w[last - i];
      p[(2 * i) + 0] *= g;
      p[(2 * i) + 1] *= g;
    }
    return;
  }
  for (std::size_t i = 0; i < fade_frames; ++i) {
    const float g = w[last - i];
    const auto base = (start + i) * channels;
    for (std::size_t ch = 0; ch < channels; ++ch)
      interleaved[base + ch] *= g;
  }
}

static inline double frame_abs_sum(std::span<const float> interleaved,
                                   std::size_t frame, std::size_t channels) {
  const auto base = frame * channels;
  if (channels == 1) {
    return std::abs(interleaved[base]);
  }
  if (channels == 2) {
    return static_cast<double>(std::abs(interleaved[base])) +
           static_cast<double>(std::abs(interleaved[base + 1]));
  }
  double s = 0.0;
  for (std::size_t ch = 0; ch < channels; ++ch)
    s += std::abs(interleaved[base + ch]);
  return s;
}

static inline std::size_t snap_start(std::span<const float> interleaved,
                                     std::size_t target,
                                     std::size_t total_frames,
                                     std::size_t channels, std::size_t search) {
  if (search == 0 || total_frames == 0)
    return std::min(target, total_frames);
  target = std::min(target, total_frames);
  const auto begin = (target > search) ? (target - search) : 0;
  const auto end = std::min(total_frames, target + search + 1);
  auto best = target;
  auto best_score = std::numeric_limits<double>::infinity();
  for (std::size_t f = begin; f < end; ++f) {
    const auto s = frame_abs_sum(interleaved, f, channels);
    if (s < best_score) {
      best_score = s;
      best = f;
    }
  }
  return best;
}

static inline std::size_t snap_end(std::span<const float> interleaved,
                                   std::size_t target_excl,
                                   std::size_t total_frames,
                                   std::size_t channels, std::size_t search) {
  if (search == 0 || total_frames == 0)
    return std::min(target_excl, total_frames);
  target_excl = std::min(target_excl, total_frames);
  const auto begin = (target_excl > search) ? (target_excl - search) : 0;
  const auto end = std::min(total_frames, target_excl + search);
  auto best = target_excl;
  auto best_score = std::numeric_limits<double>::infinity();
  for (std::size_t b = begin; b <= end; ++b) {
    double s = 0.0;
    if (b > 0)
      s += frame_abs_sum(interleaved, b - 1, channels);
    if (b < total_frames)
      s += frame_abs_sum(interleaved, b, channels);
    if (s < best_score) {
      best_score = s;
      best = b;
    }
  }
  return best;
}

static inline TrimBounds
compute_trim_bounds_rms_gate(std::span<const float> samples_interleaved,
                             std::size_t channels, std::size_t sample_rate,
                             const TrimParams &P = {}) {
  TrimBounds R{};
  if (channels == 0 || sample_rate == 0)
    return R;
  if (samples_interleaved.empty())
    return R;
  if (samples_interleaved.size() % channels != 0)
    return R;
  const auto total_frames = samples_interleaved.size() / channels;
  const auto frame_len =
      std::max<std::size_t>(1, ms_to_frames(P.frame_ms, sample_rate));
  const auto hop =
      std::max<std::size_t>(1, ms_to_frames(P.hop_ms, sample_rate));
  const auto n = (total_frames <= frame_len)
                     ? std::size_t{1}
                     : (std::size_t{1} + ((total_frames - frame_len) / hop));
  static thread_local TrimWorkspace W;
  W.db.resize(n);
  auto &db = W.db;
  for (std::size_t i = 0; i < n; ++i) {
    const auto start_frame = i * hop;
    db[i] = frame_db(samples_interleaved, start_frame, frame_len, total_frames,
                     channels);
  }
  const auto head_frames = std::min<std::size_t>(
      n, std::max<std::size_t>(std::size_t{1},
                               ms_to_frames(P.head_ms, sample_rate) / hop));
  const auto tail_frames = std::min<std::size_t>(
      n, std::max<std::size_t>(std::size_t{1},
                               ms_to_frames(P.tail_ms, sample_rate) / hop));
  const std::span<const float> head(db.data(), head_frames);
  const std::span<const float> tail(db.data() + (n - tail_frames), tail_frames);
  W.scratch.reserve(std::max(head_frames, tail_frames));
  float floor_db = std::min(percentile(head, 0.20F, W.scratch),
                            percentile(tail, 0.20F, W.scratch));
  floor_db = std::clamp(floor_db, P.min_floor_db, P.max_floor_db);
  const float open_thr = floor_db + P.open_db;
  const float close_thr = floor_db + P.close_db;
  R.noise_floor_db = floor_db;
  R.open_thr_db = open_thr;
  R.close_thr_db = close_thr;
  const auto min_on = std::max(1, P.min_speech_frames);
  const auto min_off = std::max(1, P.min_silence_frames);
  bool in_speech = false;
  int on_run = 0;
  int off_run = 0;
  std::size_t start_idx = 0;
  std::size_t end_excl_idx = n;
  bool have_start = false;
  for (std::size_t i = 0; i < n; ++i) {
    const float v = db[i];
    if (!in_speech) {
      if (v >= open_thr) {
        if (++on_run >= min_on) {
          in_speech = true;
          off_run = 0;
          const auto onset = i + 1 - static_cast<std::size_t>(min_on);
          if (!have_start) {
            start_idx = onset;
            have_start = true;
          }
          on_run = 0;
        }
      } else {
        on_run = 0;
      }
    } else {
      if (v <= close_thr) {
        if (++off_run >= min_off) {
          in_speech = false;
          on_run = 0;
          const auto silence_start = i + 1 - static_cast<std::size_t>(min_off);
          end_excl_idx = std::min(end_excl_idx, silence_start);
          off_run = 0;
        }
      } else {
        off_run = 0;
        end_excl_idx = n;
      }
    }
  }
  if (!have_start) {
    R.speech_detected = false;
    R.start_frame = 0;
    R.end_frame = total_frames;
    return R;
  }
  R.speech_detected = true;
  auto start_frame = start_idx * hop;
  auto end_frame_excl =
      (end_excl_idx >= n) ? total_frames : (end_excl_idx * hop);
  const auto preroll = ms_to_frames(P.preroll_ms, sample_rate);
  const auto postroll = ms_to_frames(P.postroll_ms, sample_rate);
  start_frame = (start_frame > preroll) ? (start_frame - preroll) : 0;
  end_frame_excl = std::min(total_frames, end_frame_excl + postroll);
  const auto search = ms_to_frames(P.boundary_search_ms, sample_rate);
  start_frame = snap_start(samples_interleaved, start_frame, total_frames,
                           channels, search);
  end_frame_excl = snap_end(samples_interleaved, end_frame_excl, total_frames,
                            channels, search);
  start_frame = std::min(start_frame, total_frames);
  end_frame_excl = std::min(end_frame_excl, total_frames);
  if (end_frame_excl <= start_frame) {
    R.start_frame = 0;
    R.end_frame = 0;
    return R;
  }
  R.start_frame = start_frame;
  R.end_frame = end_frame_excl;
  return R;
}
std::vector<float>
trim_silence_rms_gate(std::span<const float> samples_interleaved,
                      std::size_t channels, std::size_t sample_rate,
                      const TrimParams &P) {
  if (channels == 0 || sample_rate == 0)
    return {samples_interleaved.begin(), samples_interleaved.end()};
  if (samples_interleaved.empty() ||
      (samples_interleaved.size() % channels) != 0)
    return {samples_interleaved.begin(), samples_interleaved.end()};
  const auto bounds = compute_trim_bounds_rms_gate(samples_interleaved,
                                                   channels, sample_rate, P);
  if (!bounds.speech_detected)
    return {samples_interleaved.begin(), samples_interleaved.end()};
  const auto start = bounds.start_frame;
  const auto end = bounds.end_frame;
  std::vector<float> out;
  out.resize((end - start) * channels);
  std::copy(samples_interleaved.begin() +
                static_cast<std::ptrdiff_t>(start * channels),
            samples_interleaved.begin() +
                static_cast<std::ptrdiff_t>(end * channels),
            out.begin());
  const auto fade_frames = ms_to_frames(P.fade_ms, sample_rate);
  std::span<float> out_span(out);
  apply_fade_in(out_span, channels, fade_frames);
  apply_fade_out(out_span, channels, fade_frames);
  return out;
}

TrimView trim_silence_rms_gate_inplace(std::span<float> interleaved,
                                       std::size_t channels,
                                       std::size_t sample_rate,
                                       const TrimParams &P) {
  TrimView r{.view = interleaved, .speech_detected = false};
  if (channels == 0 || sample_rate == 0)
    return r;
  if (interleaved.empty() || (interleaved.size() % channels) != 0)
    return r;
  const auto bounds = compute_trim_bounds_rms_gate(
      std::span<const float>(interleaved.data(), interleaved.size()), channels,
      sample_rate, P);
  if (!bounds.speech_detected)
    return r;
  const std::size_t start = bounds.start_frame * channels;
  const std::size_t end = bounds.end_frame * channels;
  if (end <= start || end > interleaved.size())
    return r;
  r.speech_detected = true;
  r.view = interleaved.subspan(start, end - start);
  const auto fade_frames = ms_to_frames(P.fade_ms, sample_rate);
  apply_fade_in(r.view, channels, fade_frames);
  apply_fade_out(r.view, channels, fade_frames);
  return r;
}
