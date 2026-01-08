package com.github.ethindp.prism;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.media.AudioAttributes;
import android.os.Bundle;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import java.io.File;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.List;
import java.util.Locale;
import java.util.Optional;
import java.util.Set;
import java.util.stream.Collectors;
import java.util.ArrayList;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import com.snapchat.djinni.Outcome;
import java.nio.*;
import java.nio.charset.*;

public final class AndroidScreenReaderBackend extends TextToSpeechBackend {
    private String AvoidDuplicateSpeechHack = "";
    private CharsetDecoder decoder;

    @Override public String getName() {
// Todo: decide if this is conforment to the API contract
// Here, we get the (name) of the active screen reader, or fall back to just "Screen Reader".
        Context context = PrismContext.get();
        AccessibilityManager am = (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
        if (am != null && am.isEnabled()) {
            List<AccessibilityServiceInfo> serviceInfoList = am.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_SPOKEN);
            if (serviceInfoList.isEmpty()) return "Screen reader";
            if (am.isTouchExplorationEnabled()) return serviceInfoList.get(0).loadDescription(android.content.pm.PackageManager);
            for (AccessibilityServiceInfo info : serviceInfoList) {
                if (info.getId().contains("com.nirenr.talkman")) return info.loadDescription(android.content.pm.PackageManager);
            }
        }
        return "Screen Reader";
    }

    @Override
    public Outcome<Void, BackendError> initialize() {
        decoder = StandardCharsets.UTF_8.newDecoder().onMalformedInput(CodingErrorAction.REPORT).onUnmappableCharacter(CodingErrorAction.REPORT);
        Context ctx = PrismContext.get();
        AccessibilityManager am = (AccessibilityManager) ctx.getSystemService(ctx.ACCESSIBILITY_SERVICE);
        if (am == null) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
        if (!am.isEnabled()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
        var serviceInfoList = am.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_SPOKEN);
        if (serviceInfoList.isEmpty()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
        return Outcome.FromResult(null);
    }

    @Override
    public Outcome<Void, BackendError> speak(ByteBuffer text, boolean interrupt) {
        Context ctx = PrismContext.get();
        AccessibilityManager accessibilityManager = (AccessibilityManager) ctx.getSystemService(ctx.ACCESSIBILITY_SERVICE);
        if (accessibilityManager == null) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
        if (!accessibilityManager.isEnabled()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
        var serviceInfoList = accessibilityManager.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_SPOKEN);
        if (serviceInfoList.isEmpty()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
        var bb = text.asReadOnlyBuffer();
        decoder.reset();
        var out = CharBuffer.allocate(bb.capacity()*decoder.maxCharsPerByte().longValue());
        while (true) {
            var cr = decoder.decode(bb, out, true);
            if (cr.isUnderflow()) break;
            if (cr.isOverflow() || cr.isError() || cr.isMalformed() || cr.isUnmappable()) return Outcome.fromError(BackendError.INVALID_UTF8);
        }
        {
            var cr = decoder.flush(out);
            if (cr.isOverflow() || cr.isError() || cr.isMalformed() || cr.isUnmappable()) return Outcome.fromError(BackendError.INVALID_UTF8);
        }
        if (interrupt) {
            var res = stop();
            if (res.errorOrNull() != null) {
                return res;
            }
        }
        AccessibilityEvent e = new AccessibilityEvent();
        e.setEventType(AccessibilityEvent.TYPE_ANNOUNCEMENT);
        e.setPackageName(ctx.getPackageName());
        e.getText().add(out.ToString() + AvoidDuplicateSpeechHack);
        AvoidDuplicateSpeechHack += " ";
        if (AvoidDuplicateSpeechHack.length() > 100) AvoidDuplicateSpeechHack = "";
        accessibilityManager.sendAccessibilityEvent(e);
        return Outcome.fromResult(null);
    }

    @Override
    public Outcome<Void, BackendError> output(ByteBuffer text, boolean interrupt) {
        return speak(text, interrupt);
    }

    @Override
    public Outcome<Void, BackendError> stop() {
        Context ctx = PrismContext.get();
        AccessibilityManager accessibilityManager = (AccessibilityManager) ctx.getSystemService(ctx.ACCESSIBILITY_SERVICE);
        if (accessibilityManager == null) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
        if (!accessibilityManager.isEnabled()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
        var serviceInfoList = accessibilityManager.getEnabledAccessibilityServiceList(AccessibilityServiceInfo.FEEDBACK_SPOKEN);
        if (serviceInfoList.isEmpty()) return Outcome.fromError(BackendError.BACKEND_NOT_AVAILABLE);
        accessibilityManager.interrupt();
        return Outcome.FromResult(null);
    }
}
