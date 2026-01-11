// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

public abstract class TextToSpeechBackend extends AbstractTextToSpeechBackend {
  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> initialize() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> speak(
      java.nio.ByteBuffer text, boolean interrupt) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> speakToMemory(
      java.nio.ByteBuffer text, AudioCallback callback, long userdata) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> braille(java.nio.ByteBuffer text) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> output(
      java.nio.ByteBuffer text, boolean interrupt) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Boolean, BackendError> isSpeaking() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> stop() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> pause() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> resume() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> setVolume(float volume) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Float, BackendError> getVolume() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> setRate(float rate) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Float, BackendError> getRate() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> setPitch(float pitch) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Float, BackendError> getPitch() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> refreshVoices() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Long, BackendError> countVoices() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<String, BackendError> getVoiceName(long id) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<String, BackendError> getVoiceLanguage(long id) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Unit, BackendError> setVoice(long id) {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Long, BackendError> getVoice() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Long, BackendError> getChannels() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Long, BackendError> getSampleRate() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }

  @Override
  public com.snapchat.djinni.Outcome<Long, BackendError> getBitDepth() {
    return com.snapchat.djinni.Outcome.fromError(BackendError.NOT_IMPLEMENTED);
  }
}
