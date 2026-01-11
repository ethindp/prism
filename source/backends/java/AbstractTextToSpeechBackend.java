// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

public abstract class AbstractTextToSpeechBackend {
  public abstract String getName();

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> initialize();

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> speak(
      java.nio.ByteBuffer text, boolean interrupt);

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> speakToMemory(
      java.nio.ByteBuffer text, AudioCallback callback, long userdata);

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> braille(java.nio.ByteBuffer text);

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> output(
      java.nio.ByteBuffer text, boolean interrupt);

  public abstract com.snapchat.djinni.Outcome<Boolean, BackendError> isSpeaking();

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> stop();

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> pause();

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> resume();

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> setVolume(float volume);

  public abstract com.snapchat.djinni.Outcome<Float, BackendError> getVolume();

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> setRate(float rate);

  public abstract com.snapchat.djinni.Outcome<Float, BackendError> getRate();

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> setPitch(float pitch);

  public abstract com.snapchat.djinni.Outcome<Float, BackendError> getPitch();

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> refreshVoices();

  public abstract com.snapchat.djinni.Outcome<Long, BackendError> countVoices();

  public abstract com.snapchat.djinni.Outcome<String, BackendError> getVoiceName(long id);

  public abstract com.snapchat.djinni.Outcome<String, BackendError> getVoiceLanguage(long id);

  public abstract com.snapchat.djinni.Outcome<Unit, BackendError> setVoice(long id);

  public abstract com.snapchat.djinni.Outcome<Long, BackendError> getVoice();

  public abstract com.snapchat.djinni.Outcome<Long, BackendError> getChannels();

  public abstract com.snapchat.djinni.Outcome<Long, BackendError> getSampleRate();

  public abstract com.snapchat.djinni.Outcome<Long, BackendError> getBitDepth();
}
