// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

public final class TtsBackendModule {
  static {
    if (System.getProperty("java.vm.vendor").equals("The Android Project")) {
      Guard.initialize();
    }
  }

  private static final class Guard {
    private static native void initialize();
  }
}
