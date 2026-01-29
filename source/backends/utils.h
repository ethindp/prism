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

#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

struct TrimParams {
  float frame_ms = 20.0F;
  float hop_ms = 10.0F;
  float head_ms = 300.0F;
  float tail_ms = 300.0F;
  float open_db = 12.0F;
  float close_db = 8.0F;
  float min_floor_db = -75.0F;
  float max_floor_db = -35.0F;
  int min_speech_frames = 3;
  int min_silence_frames = 6;
  float preroll_ms = 25.0F;
  float postroll_ms = 40.0F;
  float fade_ms = 5.0F;
  float boundary_search_ms = 2.0F;
};

// These functions are taken from NVGT, and therefore these files are
// Zlib-licensed.
double range_convert(double old_value, double old_min, double old_max,
                     double new_min, double new_max);
float range_convert(float old_value, float old_min, float old_max,
                    float new_min, float new_max);
float range_convert_midpoint(float old_value, float old_min, float old_midpoint,
                             float old_max, float new_min, float new_midpoint,
                             float new_max);
// End NVGT code

struct TrimView {
  std::span<float> view; // points into caller memory
  bool speech_detected = false;
};

std::vector<float>
trim_silence_rms_gate(std::span<const float> samples_interleaved,
                      std::size_t channels, std::size_t sample_rate,
                      const TrimParams &P = {});

TrimView trim_silence_rms_gate_inplace(std::span<float> interleaved,
                                       std::size_t channels,
                                       std::size_t sample_rate,
                                       const TrimParams &P = {});
