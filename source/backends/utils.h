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

#include <cmath>

// These functions are taken from NVGT, and therefore these files are
// Zlib-licensed.
double range_convert(double old_value, double old_min, double old_max,
                     double new_min, double new_max);
float range_convert(float old_value, float old_min, float old_max,
                    float new_min, float new_max);
float range_convert_midpoint(float old_value, float old_min, float old_midpoint,
                             float old_max, float new_min, float new_midpoint,
                             float new_max);
