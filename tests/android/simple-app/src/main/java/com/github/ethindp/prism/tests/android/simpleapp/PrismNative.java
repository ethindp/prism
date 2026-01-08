package com.github.ethindp.prism.tests.android.simpleapp;

public final class PrismNative {
    static {
        System.loadLibrary("prism_android_test");
    }

    private PrismNative() {}

    public static native long prism_init();
    public static native void prism_shutdown(long ctx);
    public static native long prism_registry_create_best(long ctx);
    public static native void prism_backend_free(long backend);
    public static native int prism_backend_speak(long backend, String text, boolean interrupt);
}
