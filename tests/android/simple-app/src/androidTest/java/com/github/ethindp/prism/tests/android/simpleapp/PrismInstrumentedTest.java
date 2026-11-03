// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism.tests.android.simpleapp;

import static org.junit.Assert.*;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.speech.tts.TextToSpeech;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class PrismInstrumentedTest {
  // A freshly booted emulator can take far longer than Prism's timeout to
  // start the engine.
  private static final long ENGINE_READY_TIMEOUT_SECONDS = 180;

  private static boolean engineSignalled;
  private static int engineStatus = TextToSpeech.ERROR;

  private long ctx;
  private long backend;

  @BeforeClass
  public static void warmUpTtsEngine() throws InterruptedException {
    final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
    final CountDownLatch ready = new CountDownLatch(1);
    final AtomicInteger status = new AtomicInteger(TextToSpeech.ERROR);
    final AtomicReference<TextToSpeech> tts = new AtomicReference<>();
    InstrumentationRegistry.getInstrumentation()
        .runOnMainSync(
            () ->
                tts.set(
                    new TextToSpeech(
                        context,
                        result -> {
                          status.set(result);
                          ready.countDown();
                        })));
    engineSignalled = ready.await(ENGINE_READY_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    engineStatus = status.get();
    InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> tts.get().shutdown());
  }

  @Before
  public void setUp() {
    assertTrue(
        "the TTS engine did not finish initializing within "
            + ENGINE_READY_TIMEOUT_SECONDS
            + " seconds",
        engineSignalled);
    assumeTrue("this system image has no usable TTS engine", engineStatus == TextToSpeech.SUCCESS);
    ctx = PrismNative.prism_init();
    assertNotEquals("prism_init failed", 0L, ctx);
    backend = PrismNative.prism_registry_create_best(ctx);
    assertNotEquals("create_best failed", 0L, backend);
  }

  @After
  public void tearDown() {
    if (backend != 0L) PrismNative.prism_backend_free(backend);
    if (ctx != 0L) PrismNative.prism_shutdown(ctx);
  }

  @Test
  public void testSpeak() {
    int err = PrismNative.prism_backend_speak(backend, "Hello from Prism", true);
    assertEquals("speak should succeed", 0, err);
  }
}
