// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

import android.media.AudioAttributes;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.speech.tts.*;
import android.speech.tts.TextToSpeech.*;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import com.snapchat.djinni.Outcome;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.*;
import java.nio.charset.*;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

public class AndroidTextToSpeechBackend extends TextToSpeechBackend {
  private TextToSpeech tts;
  private float ttsVolume = 1.0f;
  private float ttsRate = 1.0f;
  private float ttsPitch = 1.0f;
  private boolean isTTSInitialized = false;
  private CountDownLatch isTTSInitializedLatch;
  private CharsetDecoder decoder;
  private List<Voice> voiceList;
  private ConcurrentHashMap<String, Consumer<Integer>> pendingUtterances =
      new ConcurrentHashMap<>();

  @Override
  public String getName() {
    return "Android Text to Speech";
  }

  @Override
  public Outcome<Unit, BackendError> initialize() {
    decoder =
        StandardCharsets.UTF_8
            .newDecoder()
            .onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT);
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
              tts.setOnUtteranceProgressListener(
                  new UtteranceProgressListener() {
                    @Override
                    public void onStart(String utteranceId) {}

                    @Override
                    public void onDone(String utteranceId) {
                      Consumer<Integer> callback = pendingUtterances.get(utteranceId);
                      if (callback != null) {
                        callback.accept(TextToSpeech.SUCCESS);
                      }
                    }

                    @Override
                    public void onError(String utteranceId) {
                      Consumer<Integer> callback = pendingUtterances.get(utteranceId);
                      if (callback != null) {
                        callback.accept(TextToSpeech.ERROR);
                      }
                    }
                  });
              Set<Voice> voices = tts.getVoices();
              if (voices != null) {
                voiceList = new ArrayList<>(voices);
              } else {
                voiceList = new ArrayList<>();
              }
            } else isTTSInitialized = false;
            isTTSInitializedLatch.countDown();
          }
        };
    tts = new TextToSpeech(ctx, listener);
    try {
      if (!isTTSInitializedLatch.await(10, TimeUnit.SECONDS))
        return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    } catch (InterruptedException e) {
      Thread.currentThread().interrupt();
      return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    }
    if (!isTTSInitialized) {
      return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
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
    out.flip();
    Bundle params = new Bundle();
    params.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, ttsVolume);
    if (out.remaining() >= TextToSpeech.getMaxSpeechInputLength()) {
      var segments =
          TextChunker.split(out, out.remaining(), TextToSpeech.getMaxSpeechInputLength());
      for (int segment = 0; segment < segments.size(); ++segment) {
        if (segment == 0 && interrupt) {
          if (tts.speak(segments.get(segment), TextToSpeech.QUEUE_FLUSH, params, null)
              != TextToSpeech.SUCCESS) return Outcome.fromError(BackendError.SPEAK_FAILURE);
        } else {
          if (tts.speak(segments.get(segment), TextToSpeech.QUEUE_ADD, params, null)
              != TextToSpeech.SUCCESS) return Outcome.fromError(BackendError.SPEAK_FAILURE);
        }
      }
    } else {
      if (tts.speak(
              out.toString(),
              interrupt ? TextToSpeech.QUEUE_FLUSH : TextToSpeech.QUEUE_ADD,
              params,
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
  public Outcome<Unit, BackendError> speakToMemory(
      ByteBuffer text, AudioCallback callback, long userdata) {
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
    out.flip();
    String textString = out.toString();
    ParcelFileDescriptor pfd;
    try {
      var memfd = Os.memfd_create("tts_synthesis", 0);
      pfd = ParcelFileDescriptor.dup(memfd);
      Os.close(memfd);
    } catch (ErrnoException | IOException e) {
      return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    }
    String utteranceId = UUID.randomUUID().toString();
    CountDownLatch latch = new CountDownLatch(1);
    final AtomicBoolean success = new AtomicBoolean(false);
    pendingUtterances.put(
        utteranceId,
        (status) -> {
          if (status == TextToSpeech.SUCCESS) success.set(true);
          latch.countDown();
        });
    Bundle params = new Bundle();
    params.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, ttsVolume);
    int result = tts.synthesizeToFile(textString, params, pfd, utteranceId);
    if (result != TextToSpeech.SUCCESS) {
      pendingUtterances.remove(utteranceId);
      try {
        pfd.close();
      } catch (IOException ignored) {
      }
      return Outcome.fromError(BackendError.SPEAK_FAILURE);
    }
    try {
      if (!latch.await(60, TimeUnit.SECONDS)) {
        pendingUtterances.remove(utteranceId);
        try {
          pfd.close();
        } catch (IOException ignored) {
        }
        return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
      }
    } catch (InterruptedException e) {
      pendingUtterances.remove(utteranceId);
      try {
        pfd.close();
      } catch (IOException ignored) {
      }
      Thread.currentThread().interrupt();
      return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    }
    pendingUtterances.remove(utteranceId);
    if (!success.get()) {
      try {
        pfd.close();
      } catch (IOException ignored) {
      }
      return Outcome.fromError(BackendError.SPEAK_FAILURE);
    }
    try {
      Os.lseek(pfd.getFileDescriptor(), 0, OsConstants.SEEK_SET);
    } catch (ErrnoException e) {
      try {
        pfd.close();
      } catch (IOException ignored) {
      }
      return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    }
    try (FileInputStream fis = new FileInputStream(pfd.getFileDescriptor())) {
      byte[] header = new byte[44];
      if (fis.read(header) != 44) {
        return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
      }
      ByteBuffer headerBuf = ByteBuffer.wrap(header).order(ByteOrder.LITTLE_ENDIAN);
      short channels = headerBuf.getShort(22);
      int sampleRate = headerBuf.getInt(24);
      short bitsPerSample = headerBuf.getShort(34);
      if (bitsPerSample != 16) {
        return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
      }
      byte[] data;
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        data = fis.readAllBytes();
      } else {
        int dataSize = (int) (pfd.getStatSize() - 44);
        data = new byte[dataSize];
        int totalRead = 0;
        while (totalRead < dataSize) {
          int read = fis.read(data, totalRead, dataSize - totalRead);
          if (read == -1) break;
          totalRead += read;
        }
      }
      ByteBuffer pcmBuf = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
      ShortBuffer shortBuf = pcmBuf.asShortBuffer();
      int sampleCount = shortBuf.remaining();
      ByteBuffer floatBytes = ByteBuffer.allocate(sampleCount * 4).order(ByteOrder.nativeOrder());
      FloatBuffer floatBuf = floatBytes.asFloatBuffer();
      for (int i = 0; i < sampleCount; i++) {
        float sample = shortBuf.get(i) / 32768.0f;
        floatBuf.put(sample);
      }
      callback.onAudio(userdata, floatBytes, sampleCount / channels, channels, sampleRate);
    } catch (IOException e) {
      return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    } finally {
      try {
        pfd.close();
      } catch (IOException ignored) {
      }
    }
    return Outcome.fromResult(new Unit());
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

  @Override
  public Outcome<Unit, BackendError> setRate(float rate) {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    if (rate < 0.0f || rate > 1.0f) return Outcome.fromError(BackendError.RANGE_OUT_OF_BOUNDS);
    ttsRate = Utils.rangeConvertMidpoint(rate, 0.0f, 0.5f, 1.0f, 0.1f, 3.05f, 6.0f);
    if (tts.setSpeechRate(rate) == TextToSpeech.ERROR) {
      return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    }
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Float, BackendError> getRate() {
    return Outcome.fromResult(
        Utils.rangeConvertMidpoint(ttsRate, 0.1f, 3.05f, 6.0f, 0.0f, 0.5f, 1.0f));
  }

  @Override
  public Outcome<Unit, BackendError> setPitch(float pitch) {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    if (pitch < 0.0f || pitch > 1.0f) return Outcome.fromError(BackendError.RANGE_OUT_OF_BOUNDS);
    ttsPitch = Utils.rangeConvertMidpoint(pitch, 0.0f, 0.5f, 1.0f, 0.25f, 2.125f, 4.0f);
    if (tts.setPitch(pitch) == TextToSpeech.ERROR) {
      return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    }
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Float, BackendError> getPitch() {
    return Outcome.fromResult(
        Utils.rangeConvertMidpoint(ttsPitch, 0.25f, 2.125f, 4.0f, 0.0f, 0.5f, 1.0f));
  }

  @Override
  public Outcome<Unit, BackendError> setVolume(float volume) {
    if (volume < 0.0f || volume > 1.0f) return Outcome.fromError(BackendError.RANGE_OUT_OF_BOUNDS);
    ttsVolume = volume;
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Float, BackendError> getVolume() {
    return Outcome.fromResult(ttsVolume);
  }

  @Override
  public Outcome<Unit, BackendError> refreshVoices() {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    Set<Voice> voices = tts.getVoices();
    if (voices != null) {
      voiceList = new ArrayList<>(voices);
    } else {
      voiceList = new ArrayList<>();
    }
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Long, BackendError> countVoices() {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    if (voiceList == null) return Outcome.fromResult(0L);
    return Outcome.fromResult((long) voiceList.size());
  }

  @Override
  public Outcome<String, BackendError> getVoiceName(long id) {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    if (voiceList == null || id < 0 || id >= voiceList.size())
      return Outcome.fromError(BackendError.RANGE_OUT_OF_BOUNDS);
    return Outcome.fromResult(voiceList.get((int) id).getName());
  }

  @Override
  public Outcome<String, BackendError> getVoiceLanguage(long id) {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    if (voiceList == null || id < 0 || id >= voiceList.size())
      return Outcome.fromError(BackendError.RANGE_OUT_OF_BOUNDS);
    return Outcome.fromResult(voiceList.get((int) id).getLocale().toLanguageTag());
  }

  @Override
  public Outcome<Unit, BackendError> setVoice(long id) {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    if (voiceList == null || id < 0 || id >= voiceList.size())
      return Outcome.fromError(BackendError.RANGE_OUT_OF_BOUNDS);
    Voice v = voiceList.get((int) id);
    if (tts.setVoice(v) == TextToSpeech.ERROR) {
      return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    }
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Long, BackendError> getVoice() {
    if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    Voice current = tts.getVoice();
    if (current == null || voiceList == null)
      return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    int index = voiceList.indexOf(current);
    if (index == -1) return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
    return Outcome.fromResult((long) index);
  }
}
