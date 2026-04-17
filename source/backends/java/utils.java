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

  public static float expRangeConvert(float t, float outMin, float outMid, float outMax) {
    final float logMin = (float) Math.log(outMin);
    final float logMid = (float) Math.log(outMid);
    final float logMax = (float) Math.log(outMax);
    return (float)
        Math.exp(
            (t <= 0.5f)
                ? logMin + (logMid - logMin) * (t / 0.5f)
                : logMid + (logMax - logMid) * ((t - 0.5f) / 0.5f));
  }

  public static float expRangeConvertInv(float val, float outMin, float outMid, float outMax) {
    final float logMin = (float) Math.log(outMin);
    final float logMid = (float) Math.log(outMid);
    final float logMax = (float) Math.log(outMax);
    final float logVal = (float) Math.log(val < outMin ? outMin : (val > outMax ? outMax : val));
    return ((logVal <= logMid)
        ? 0.5f * (logVal - logMin) / (logMid - logMin)
        : 0.5f + (0.5f * (logVal - logMid) / (logMax - logMid)));
  }
}
