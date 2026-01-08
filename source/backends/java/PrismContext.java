// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

import android.content.Context;

public final class PrismContext {
  private static volatile Context appContext;

  private PrismContext() {}

  public static void set(Context context) {
    if (context == null) return;
    appContext = context.getApplicationContext();
  }

  public static Context get() {
    Context c = appContext;
    if (c == null) {
      throw new IllegalStateException(
          "PrismContext not initialized. Ensure the library manifest is merged (PrismInitProvider).");
    }
    return c;
  }
}
