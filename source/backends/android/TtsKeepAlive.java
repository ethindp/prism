// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

import android.content.Context;
import android.speech.tts.TextToSpeech;

/**
 * Keeps one TTS engine connection open for the life of the process. Engines tear their service
 * down when the last client disconnects, and rebuilding it takes seconds; with a connection open, a
 * new client is ready in tens of milliseconds.
 */
final class TtsKeepAlive {
  private static final Object lock = new Object();
  private static TextToSpeech connection;
  // Android reports a failure to bind before the constructor returns.
  private static boolean failedWhileConnecting;

  private TtsKeepAlive() {}

  static void start(Context context) {
    synchronized (lock) {
      if (connection != null) return;
      failedWhileConnecting = false;
      TextToSpeech candidate;
      try {
        candidate = new TextToSpeech(context.getApplicationContext(), TtsKeepAlive::onInit);
      } catch (RuntimeException e) {
        return;
      }
      if (failedWhileConnecting) {
        release(candidate);
        return;
      }
      connection = candidate;
    }
  }

  private static void onInit(int status) {
    if (status == TextToSpeech.SUCCESS) return;
    synchronized (lock) {
      if (connection == null) {
        failedWhileConnecting = true;
        return;
      }
      release(connection);
      connection = null;
    }
  }

  private static void release(TextToSpeech tts) {
    try {
      tts.shutdown();
    } catch (RuntimeException ignored) {
    }
  }
}
