package com.github.ethindp.prism.tests.android.simpleapp;

import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

public final class MainActivity extends AppCompatActivity {
    private long prismCtx = 0L;
    private long backend = 0L;

    private TextView status;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        EditText input = findViewById(R.id.inputText);
        Button speak = findViewById(R.id.speakButton);
        status = findViewById(R.id.statusText);

        tryInit();

        speak.setOnClickListener(v -> {
            if (backend == 0L) {
                status.setText("Status: backend not available");
                return;
            }

            String text = input.getText().toString();
            if (text.isEmpty()) {
                status.setText("Status: enter some text");
                return;
            }

            // interrupt=true: stop current speech and start new
            int err = PrismNative.prism_backend_speak(backend, text, true);
            status.setText("Status: speak() -> " + err);
        });
    }

    private void tryInit() {
        new Thread(() -> {
            prismCtx = PrismNative.prism_init();
            if (prismCtx == 0L) {
                runOnUiThread(() -> status.setText("Status: prism_init() failed (ctx=0)"));
                return;
            }

            backend = PrismNative.prism_registry_create_best(prismCtx);
            if (backend == 0L) {
                runOnUiThread(() -> status.setText("Status: create_best() failed (backend=0)"));
                return;
            }

            runOnUiThread(() -> status.setText("Status: initialized (best backend created)"));
        }).start();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (backend != 0L) {
            PrismNative.prism_backend_free(backend);
            backend = 0L;
        }
        if (prismCtx != 0L) {
            PrismNative.prism_shutdown(prismCtx);
            prismCtx = 0L;
        }
    }
}
