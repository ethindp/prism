// SPDX-License-Identifier: MPL-2.0
package com.github.ethindp.prism;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.Context;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import com.snapchat.djinni.Outcome;
import java.nio.*;
import java.nio.charset.*;
import java.util.List;

public final class AndroidScreenReaderBackend extends TextToSpeechBackend {
  private String AvoidDuplicateSpeechHack = "";
  private CharsetDecoder decoder;

  @Override
  public String getName() {
    // Todo: decide if this is conforment to the API contract
    // Here, we get the (name) of the active screen reader, or fall back to just "Screen Reader".
    Context context = PrismContext.get();
    AccessibilityManager am =
        (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
    if (am != null && am.isEnabled()) {
      List<AccessibilityServiceInfo> serviceInfoList =
          am.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_SPOKEN);
      if (serviceInfoList.isEmpty()) return "Screen reader";
      if (am.isTouchExplorationEnabled())
        return serviceInfoList.get(0).loadDescription(context.getPackageManager());
      for (AccessibilityServiceInfo info : serviceInfoList) {
        if (info.getId().contains("com.nirenr.talkman"))
          return info.loadDescription(context.getPackageManager());
      }
    }
    return "Screen Reader";
  }

  @Override
  public Outcome<Unit, BackendError> initialize() {
    decoder =
        StandardCharsets.UTF_8
            .newDecoder()
            .onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT);
    Context ctx = PrismContext.get();
    AccessibilityManager am =
        (AccessibilityManager) ctx.getSystemService(ctx.ACCESSIBILITY_SERVICE);
    if (am == null) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    if (!am.isEnabled()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    var serviceInfoList =
        am.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_SPOKEN);
    if (serviceInfoList.isEmpty()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Unit, BackendError> speak(ByteBuffer text, boolean interrupt) {
    Context ctx = PrismContext.get();
    AccessibilityManager accessibilityManager =
        (AccessibilityManager) ctx.getSystemService(ctx.ACCESSIBILITY_SERVICE);
    if (accessibilityManager == null) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    if (!accessibilityManager.isEnabled())
      return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    var serviceInfoList =
        accessibilityManager.getEnabledAccessibilityServiceList(
            AccessibilityServiceInfo.FEEDBACK_SPOKEN);
    if (serviceInfoList.isEmpty()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
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
    if (interrupt) {
      var res = stop();
      if (res.errorOrNull() != null) {
        return res;
      }
    }
    AccessibilityEvent e = new AccessibilityEvent();
    e.setEventType(AccessibilityEvent.TYPE_ANNOUNCEMENT);
    e.setPackageName(ctx.getPackageName());
    e.getText().add(out.toString() + AvoidDuplicateSpeechHack);
    AvoidDuplicateSpeechHack += " ";
    if (AvoidDuplicateSpeechHack.length() > 100) AvoidDuplicateSpeechHack = "";
    accessibilityManager.sendAccessibilityEvent(e);
    return Outcome.fromResult(new Unit());
  }

  @Override
  public Outcome<Unit, BackendError> output(ByteBuffer text, boolean interrupt) {
    return speak(text, interrupt);
  }

  @Override
  public Outcome<Unit, BackendError> stop() {
    Context ctx = PrismContext.get();
    AccessibilityManager accessibilityManager =
        (AccessibilityManager) ctx.getSystemService(ctx.ACCESSIBILITY_SERVICE);
    if (accessibilityManager == null) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    if (!accessibilityManager.isEnabled())
      return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    var serviceInfoList =
        accessibilityManager.getEnabledAccessibilityServiceList(
            AccessibilityServiceInfo.FEEDBACK_SPOKEN);
    if (serviceInfoList.isEmpty()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
    accessibilityManager.interrupt();
    return Outcome.fromResult(new Unit());
  }
}
