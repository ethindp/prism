// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

import android.media.AudioAttributes;
import android.speech.tts.*;
import android.speech.tts.TextToSpeech.*;
import com.snapchat.djinni.Outcome;
import java.nio.*;
import java.nio.charset.*;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class AndroidTextToSpeechBackend extends TextToSpeechBackend {
  private TextToSpeech tts;
  private float ttsVolume = 1.0f;
  private float ttsRate = 1.0f;
  private float ttsPitch = 1.0f;
  private boolean isTTSInitialized = false;
  private CountDownLatch isTTSInitializedLatch;
  private CharsetDecoder decoder;
  private Set<Voice> voices;
  private CountDownLatch pcmSynthesisLatch;

  @Override
  public String getName() {
    return "Android Text to Speech";
  }

  @Override
  public Outcome<Unit, BackendError> initialize() {
    var ctx = PrismContext.get();
    isTTSInitializedLatch = new CountDownLatch(1);
    OnInitListener listener =
        new OnInitListener() {
          @Override
          public void onInit(int status) {
            if (status == TextToSpeech.SUCCESS) {
              isTTSInitialized = true;
              try {
                tts.setLanguage(Locale.getDefault());
              } catch (Exception e) {
              }
              tts.setPitch(1.0f);
              tts.setSpeechRate(1.0f);
              AudioAttributes audioAttributes =
                  new AudioAttributes.Builder()
                      .setUsage(AudioAttributes.USAGE_ASSISTANCE_ACCESSIBILITY)
                      .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                      .build();
              tts.setAudioAttributes(audioAttributes);
              voices = tts.getVoices();
            } else isTTSInitialized = false;
            isTTSInitializedLatch.countDown();
          }
        };
    tts = new TextToSpeech(ctx, listener);
    try {
      if (!isTTSInitializedLatch.await(10, TimeUnit.SECONDS))
        return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    } catch (InterruptedException e) {
    }
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Unit, BackendError> speak(ByteBuffer text, boolean interrupt) {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    var bb = text.asReadOnlyBuffer();
    decoder.reset();
    var out =
        CharBuffer.allocate(bb.capacity() * Float.valueOf(decoder.maxCharsPerByte()).intValue());
    while (true) {
      var cr = decoder.decode(bb, out, true);
      if (cr.isUnderflow()) break;
      if (cr.isOverflow() || cr.isError() || cr.isMalformed() || cr.isUnmappable())
        return Outcome.fromError(BackendError.INVALID_UTF8);
    }
    {
      var cr = decoder.flush(out);
      if (cr.isOverflow() || cr.isError() || cr.isMalformed() || cr.isUnmappable())
        return Outcome.fromError(BackendError.INVALID_UTF8);
    }
    if (out.capacity() >= TextToSpeech.getMaxSpeechInputLength()) {
      var segments = TextChunker.split(out, 512, TextToSpeech.getMaxSpeechInputLength());
      for (int segment = 0; segment < segments.size(); ++segment) {
        if (segment == 0 && interrupt) {
          if (tts.speak(segments.get(segment), TextToSpeech.QUEUE_FLUSH, null, null)
              != TextToSpeech.SUCCESS) return Outcome.fromError(BackendError.SPEAK_FAILURE);
        } else {
          if (tts.speak(segments.get(segment), TextToSpeech.QUEUE_ADD, null, null)
              != TextToSpeech.SUCCESS) return Outcome.fromError(BackendError.SPEAK_FAILURE);
        }
      }
    } else {
      if (tts.speak(
              out.toString(),
              interrupt ? TextToSpeech.QUEUE_FLUSH : TextToSpeech.QUEUE_ADD,
              null,
              null)
          != TextToSpeech.SUCCESS) return Outcome.fromError(BackendError.SPEAK_FAILURE);
    }
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Unit, BackendError> output(ByteBuffer text, boolean interrupt) {
    return speak(text, interrupt);
  }

  @Override
  public Outcome<Boolean, BackendError> isSpeaking() {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    return Outcome.fromResult(tts.isSpeaking());
  }

  @Override
  public Outcome<Unit, BackendError> stop() {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    if (tts.stop() != TextToSpeech.SUCCESS)
      return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    return Outcome.fromResult(new Unit());
  }
}
