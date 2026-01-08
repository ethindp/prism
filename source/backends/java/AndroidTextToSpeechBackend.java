package com.github.ethindp.prism;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.media.AudioAttributes;
import android.os.Bundle;
import android.speech.tts.*;
import android.speech.tts.TextToSpeech.*;
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

    @Override public String getName() {
        return "Android Text to Speech";
    }

    @Override
    public Outcome<Void, BackendError> initialize() {
        var ctx = PrismContext.get();
        isTTSInitializedLatch = new CountDownLatch(1);
        OnInitListener listener = new OnInitListener() {
            @Override
            public void onInit(int status) {
                if (status == SUCCESS) {
                    isTTSInitialized = true;
                    try {
                        tts.setLanguage(Locale.getDefault());
                    } catch (Exception e) {}
                    tts.setPitch(1.0f);
                    tts.setSpeechRate(1.0f);
                    AudioAttributes audioAttributes = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_ASSISTANCE_ACCESSIBILITY).setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build();
                    tts.setAudioAttributes(audioAttributes);
                    voices = tts.getVoices();
                    for (var voice: voices) {
                        if (voice.isNetworkConnectionRequired() || voice.getFeatures().contains("notInstalled")) voices.remove(voice);
                    }
                } else
                    isTTSInitialized = false;
                isTTSInitializedLatch.countDown();
            }
        };
        tts = new TextToSpeech(ctx, listener);
        try {
            if (!isTTSInitializedLatch.await(10, TimeUnit.SECONDS)) return Outcome.FromError(BackendError.BACKEND_NOT_AVAILABLE);
        } catch (InterruptedException e) {}
        return Outcome.FromResult(null);
    }

    @Override
    public Outcome<Void, BackendError> speak(ByteBuffer text, boolean interrupt) {
        if (!isTTSInitialized) return Outcome.FromError(BackendError.NOT_INITIALIZED);
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
        if (out.capacity() >= TextToSpeech.getMaxSpeechInputLength()) {
            var segments = TextChunker.split(out, 512, TextToSpeech.getMaxSpeechInputLength());
            for (int segment = 0; segment < segments.size(); ++segment) {
                if (segment == 0 && interrupt) {
                    if (tts.speak(segments[segment], QUEUE_FLUSH, null, null) != SUCCESS) return Outcome.fromError(BackendError.SPEAK_FAILED);
                } else {
                    if (tts.speak(segments[segment], QUEUE_ADD, null, null) != SUCCESS) return Outcome.fromError(BackendError.SPEAK_FAILED);
                }
            }
        } else {
            if (tts.speak(segments[segment], interrupt ? QUEUE_FLUSH : QUEUE_ADD, null, null) != SUCCESS) return Outcome.fromError(BackendError.SPEAK_FAILED);
        }
        return Outcome.fromResult(null);
    }

    @Override
    public Outcome<Void, BackendError> output(ByteBuffer text, boolean interrupt) {
        return speak(text, interrupt);
    }

    @Override
    public Outcome<Boolean, BackendError> isSpeaking() {
        if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
        return outcome.fromResult(tts.isSpeaking());
    }

    @Override
    public Outcome<Void, BackendError> stop() {
        if (!isTTSInitialized) return Outcome.fromError(BackendError.NOT_INITIALIZED);
        if (tts.stop() != SUCCESS) return Outcome.fromError(BackendError.INTERNAL_BACKEND_ERROR);
        return Outcome.fromResult(null);
    }
}
