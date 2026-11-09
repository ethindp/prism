// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import com.snapchat.djinni.Outcome;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.EnumSet;

public final class AndroidClipboardBackend extends TextToSpeechBackend {
  private ClipboardManager clipboard;

  private static ClipboardManager findClipboard() {
    try {
      return (ClipboardManager) PrismContext.get().getSystemService(Context.CLIPBOARD_SERVICE);
    } catch (IllegalStateException e) {
      return null;
    }
  }

  @Override
  public String getName() {
    return "Clipboard";
  }

  @Override
  public EnumSet<BackendFeatures> getFeatures() {
    final var features =
        EnumSet.of(BackendFeatures.SUPPORTS_SPEAK, BackendFeatures.SUPPORTS_OUTPUT);
    if (clipboard != null || findClipboard() != null) {
      features.add(BackendFeatures.IS_SUPPORTED_AT_RUNTIME);
    }
    return features;
  }

  @Override
  public EnumSet<BackendFeaturesHigh> getFeaturesHigh() {
    return EnumSet.of(BackendFeaturesHigh.REQUIRES_EXPLICIT_SELECTION);
  }

  @Override
  public Outcome<Unit, BackendError> initialize() {
    if (clipboard != null) return Outcome.fromError(BackendError.ALREADY_INITIALIZED);
    clipboard = findClipboard();
    if (clipboard == null) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Unit, BackendError> speak(ByteBuffer text, boolean interrupt) {
    if (clipboard == null) return Outcome.fromError(BackendError.NOT_INITIALIZED);
    final String value = StandardCharsets.UTF_8.decode(text.asReadOnlyBuffer()).toString();
    try {
      clipboard.setPrimaryClip(ClipData.newPlainText("Prism", value));
    } catch (RuntimeException e) {
      return Outcome.fromError(BackendError.SPEAK_FAILURE);
    }
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Unit, BackendError> output(ByteBuffer text, boolean interrupt) {
    return speak(text, interrupt);
  }
}
