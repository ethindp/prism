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
#include <utility>

// Begin NVGT code
double range_convert(double old_value, double old_min, double old_max,
                     double new_min, double new_max) {
  return (((old_value - old_min) / (old_max - old_min)) * (new_max - new_min)) +
         new_min;
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

struct TrimBounds {
  std::size_t start_frame = 0;
  std::size_t end_frame = 0;
  bool speech_detected = false;
  float noise_floor_db = -160.0F;
  float open_thr_db = -160.0F;
  float close_thr_db = -160.0F;
};

static inline std::size_t ms_to_frames(float ms, std::size_t sample_rate) {
  const auto f =
      (static_cast<double>(ms) * static_cast<double>(sample_rate)) / 1000.0;
  return static_cast<std::size_t>(std::max(0.0, std::floor(f + 0.5)));
}

static inline float rms_to_db(double rms) {
  constexpr double eps = 1e-8;
  return static_cast<float>(20.0 * std::log10(rms + eps));
}

static inline double frame_rms(std::span<const float> interleaved,
                               std::size_t start_frame, std::size_t frame_len,
                               std::size_t total_frames, std::size_t channels) {
  const auto end_frame = std::min(start_frame + frame_len, total_frames);
  if (end_frame <= start_frame)
    return 0.0;
  auto sumsq = 0.0;
  auto count = 0;
  for (std::size_t f = start_frame; f < end_frame; ++f) {
    const auto base = f * channels;
    for (std::size_t ch = 0; ch < channels; ++ch) {
      const double v = interleaved[base + ch];
      sumsq += v * v;
    }
    ++count;
  }
  const auto denom = static_cast<double>(count) * static_cast<double>(channels);
  return std::sqrt(sumsq / denom);
}

static inline float percentile(std::span<const float> x, float p) {
  if (x.empty())
    return -160.0F;
  p = std::clamp(p, 0.0F, 1.0F);
  std::vector<float> tmp(x.begin(), x.end());
  const std::size_t n = tmp.size();
  if (n == 1)
    return tmp[0];
  const float pos = p * static_cast<float>(n - 1);
  const auto k = static_cast<std::size_t>(std::floor(pos));
  std::nth_element(tmp.begin(), tmp.begin() + static_cast<std::ptrdiff_t>(k),
                   tmp.end());
  return tmp[k];
}

static inline void apply_fade_in(std::span<float> interleaved,
                                 std::size_t channels,
                                 std::size_t fade_frames) {
  if (fade_frames == 0)
    return;
  const auto total_frames = interleaved.size() / channels;
  fade_frames = std::min(fade_frames, total_frames);
  if (fade_frames == 0)
    return;
  for (std::size_t i = 0; i < fade_frames; ++i) {
    float g = 1.0F;
    if (fade_frames > 1) {
      const auto t =
          static_cast<float>(i) / static_cast<float>(fade_frames - 1);
      g = 0.5F - 0.5F * std::cos(std::numbers::pi_v<float> * t);
    }
    const auto base = i * channels;
    for (std::size_t ch = 0; ch < channels; ++ch)
      interleaved[base + ch] *= g;
  }
}

static inline void apply_fade_out(std::span<float> interleaved,
                                  std::size_t channels,
                                  std::size_t fade_frames) {
  if (fade_frames == 0)
    return;
  const auto total_frames = interleaved.size() / channels;
  fade_frames = std::min(fade_frames, total_frames);
  if (fade_frames == 0)
    return;
  const auto start = total_frames - fade_frames;
  for (std::size_t i = 0; i < fade_frames; ++i) {
    float g = 1.0F;
    if (fade_frames > 1) {
      const float t =
          static_cast<float>(i) / static_cast<float>(fade_frames - 1);
      g = 0.5F + 0.5F * std::cos(std::numbers::pi_v<float> * t);
    } else {
      g = 0.0F;
    }
    const auto base = (start + i) * channels;
    for (std::size_t ch = 0; ch < channels; ++ch)
      interleaved[base + ch] *= g;
  }
}

static inline double frame_abs_sum(std::span<const float> interleaved,
                                   std::size_t frame, std::size_t channels) {
  const auto base = frame * channels;
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
  std::vector<float> db(n);
  for (std::size_t i = 0; i < n; ++i) {
    const auto start_frame = i * hop;
    const auto rms = frame_rms(samples_interleaved, start_frame, frame_len,
                               total_frames, channels);
    db[i] = rms_to_db(rms);
  }
  const auto head_frames = std::min<std::size_t>(
      n, std::max<std::size_t>(std::size_t{1},
                               ms_to_frames(P.head_ms, sample_rate) / hop));
  const auto tail_frames = std::min<std::size_t>(
      n, std::max<std::size_t>(std::size_t{1},
                               ms_to_frames(P.tail_ms, sample_rate) / hop));
  const std::span<const float> head(db.data(), head_frames);
  const std::span<const float> tail(db.data() + (n - tail_frames), tail_frames);
  float floor_db = std::min(percentile(head, 0.20F), percentile(tail, 0.20F));
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
    const auto v = db[i];
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
  if (samples_interleaved.empty() || samples_interleaved.size() % channels != 0)
    return {samples_interleaved.begin(), samples_interleaved.end()};
  const auto bounds = compute_trim_bounds_rms_gate(samples_interleaved,
                                                   channels, sample_rate, P);
  if (!bounds.speech_detected) {
    return {samples_interleaved.begin(), samples_interleaved.end()};
  }
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
