// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

final class Utils {
  public static double rangeConvert(
      double old_value, double old_min, double old_max, double new_min, double new_max) {
    return ((old_value - old_min) / (old_max - old_min)) * (new_max - new_min) + new_min;
  }

  public static float rangeConvert(
      float old_value, float old_min, float old_max, float new_min, float new_max) {
    return ((old_value - old_min) / (old_max - old_min)) * (new_max - new_min) + new_min;
  }

  public static float rangeConvertMidpoint(
      float old_value,
      float old_min,
      float old_midpoint,
      float old_max,
      float new_min,
      float new_midpoint,
      float new_max) {
    if (old_value <= old_midpoint)
      return rangeConvert(old_value, old_min, old_midpoint, new_min, new_midpoint);
    else return rangeConvert(old_value, old_midpoint, old_max, new_midpoint, new_max);
  }
}
