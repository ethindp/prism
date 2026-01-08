// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

import com.snapchat.djinni.NativeObjectManager;
import java.util.concurrent.atomic.AtomicBoolean;

public abstract class AudioCallback {
  public abstract void onAudio(
      long userdata, java.nio.ByteBuffer samples, long sampleCount, long channels, long sampleRate);

  public static final class CppProxy extends AudioCallback {
    static {
      try {
        Class.forName("com.github.ethindp.prism.TtsBackendModule");
      } catch (ClassNotFoundException e) {
        throw new IllegalStateException("Failed to initialize djinni module", e);
      }
    }

    private final long nativeRef;
    private final AtomicBoolean destroyed = new AtomicBoolean(false);

    private CppProxy(long nativeRef) {
      if (nativeRef == 0) throw new RuntimeException("nativeRef is zero");
      this.nativeRef = nativeRef;
      NativeObjectManager.register(this, nativeRef);
    }

    public static native void nativeDestroy(long nativeRef);

    @Override
    public void onAudio(
        long userdata,
        java.nio.ByteBuffer samples,
        long sampleCount,
        long channels,
        long sampleRate) {
      assert !this.destroyed.get() : "trying to use a destroyed object";
      native_onAudio(this.nativeRef, userdata, samples, sampleCount, channels, sampleRate);
    }

    private native void native_onAudio(
        long _nativeRef,
        long userdata,
        java.nio.ByteBuffer samples,
        long sampleCount,
        long channels,
        long sampleRate);
  }
}
