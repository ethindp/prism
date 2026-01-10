package com.github.ethindp.prism.tests.android.simpleapp;

import static org.junit.Assert.*;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import androidx.test.ext.junit.runners.AndroidJUnit4;

@RunWith(AndroidJUnit4.class)
public class PrismInstrumentedTest {
    private long ctx;
    private long backend;

    @Before
    public void setUp() {
        ctx = PrismNative.prism_init();
        assertNotEquals("prism_init failed", 0L, ctx);
        backend = PrismNative.prism_registry_create_best(ctx);
        assertNotEquals("create_best failed", 0L, backend);
    }

    @After
    public void tearDown() {
        if (backend != 0L) PrismNative.prism_backend_free(backend);
        if (ctx != 0L) PrismNative.prism_shutdown(ctx);
    }

    @Test
    public void testSpeak() {
        int err = PrismNative.prism_backend_speak(backend, "Hello from Prism", true);
        assertEquals("speak should succeed", 0, err);
    }
}