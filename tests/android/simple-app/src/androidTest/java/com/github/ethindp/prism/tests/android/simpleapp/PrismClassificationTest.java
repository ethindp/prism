// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism.tests.android.simpleapp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class PrismClassificationTest {
  private long ctx;

  @Before
  public void setUp() {
    ctx = PrismNative.prism_init();
    assertNotEquals("prism_init failed", 0L, ctx);
  }

  @After
  public void tearDown() {
    if (ctx != 0L) PrismNative.prism_shutdown(ctx);
  }

  @Test
  public void everyBackendIsExactlyOneKind() {
    assertTrue(
        "both Android backends should be registered", PrismNative.prism_registry_count(ctx) >= 2);
    assertEquals(
        "backends that are not exactly one of screen reader or speech engine",
        0,
        PrismNative.classification_failures(ctx));
  }
}
