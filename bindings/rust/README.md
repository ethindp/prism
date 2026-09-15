# prismatoid

[![License: MPL 2.0](https://img.shields.io/badge/License-MPL_2.0-blue.svg)](https://opensource.org/licenses/MPL-2.0)

Idiomatic, memory-safe Rust bindings for [**Prism**](https://github.com/ethindp/prism) (Platform-agnostic Reader Interface for Speech and Messages).

Prism provides a unified interface for speech synthesis and screen reader output across Windows, macOS, Linux, Android, and WebAssembly. `prismatoid` brings the full power of Prism to the Rust ecosystem with modern ergonomic designs, RAII lifetime safety, typed error handling, bitflag feature discovery, zero-copy in-memory audio buffers, and safe custom backend registration.

---

## Architecture

The Rust bindings are organized as a workspace with two crates:

- **`prismatoid`**: High-level, safe, and idiomatic Rust interface.
- **`prismatoid-sys`**: Low-level raw C FFI declarations generated against `prism.h`.

---

## Features

- **Memory-Safe RAII:** Automatic cleanup of contexts, backends, custom registries, and strings.
- **Screen Reader & TTS Abstraction:** First-class support for NVDA, JAWS, Windows OneCore, SAPI, Apple AVFoundation, VoiceOver, Linux Speech Dispatcher, Orca, Android TTS, and more.
- **In-Memory Audio Synthesis:** Stream float PCM samples or collect them directly into an [`AudioBuffer`](#in-memory-audio-synthesis) with built-in 16-bit PCM and standard WAV container export.
- **Dynamic Availability:** Non-blocking background worker tracks screen reader and TTS availability with configurable debounce and exponential backoff.
- **Extensible Backends:** Register custom speech engines written in pure Rust using the `CustomBackend` trait and `RegistryBuilder`.
- **Structured Logging:** Integrated diagnostics with optional bridge to Rust's standard `log` crate.
- **Offline Mock Support:** Full in-Rust mock suite (`mock` feature flag) allowing unit and integration testing without compiling the native C++ library.

---

## Installation

Add `prismatoid` to your `Cargo.toml`:

```toml
[dependencies]
prismatoid = "0.18.2"
```

To enable the bridge forwarding Prism log diagnostics to the `log` facade:

```toml
[dependencies]
prismatoid = { version = "0.18.2", features = ["log"] }
```

### Linking against the Native Prism Library

`prismatoid-sys` locates the native Prism dynamic or static library using the following search order:

1. **`PRISM_LIB_DIR`**: Set this environment variable to the directory containing `prism.lib`, `prism.dll`, `libprism.so`, or `libprism.dylib`.
2. **In-tree build directories**: Probes `build/`, `dist/`, or CMake release folders automatically if building within the Prism repository.
3. **Static Linking**: Set `PRISM_STATIC=1` to link against the static archive instead of the dynamic library.
4. **Mock Feature (for CI/Testing)**: Enable `features = ["mock"]` to run pure-Rust tests without requiring a compiled C++ binary.

---

## Quick Start

### Basic Speech Output

```rust
use prismatoid::{init, Result};

fn main() -> Result<()> {
    // Initialize Prism with default configuration
    let ctx = init()?;

    // Acquire the highest-priority available backend (e.g. active screen reader)
    let mut backend = ctx.create_best()?;
    println!("Active backend: {}", backend.name()?);

    // Speak a message (interrupting any previous speech)
    backend.speak("Hello from Prism and Rust!", true)?;

    Ok(())
}
```

### Querying Voices and Parameters

```rust
use prismatoid::{init, Result};

fn main() -> Result<()> {
    let ctx = init()?;
    let mut backend = ctx.create_best()?;

    // Adjust volume, speaking rate, and pitch
    backend.set_volume(0.8)?;
    backend.set_rate(1.2)?;
    backend.set_pitch(1.0)?;

    // List installed voices
    for voice in backend.voices()? {
        println!("Voice {}: {} [{}]", voice.id, voice.name, voice.language);
    }

    // Search and select a voice
    if let Some(voice) = backend.find_voice_by_language("en")? {
        backend.set_voice(voice.id)?;
    }

    backend.speak("Customized voice speech.", false)?;
    Ok(())
}
```

---

## In-Memory Audio Synthesis

Prism supports synthesizing speech directly to memory without playing it to an output device. `prismatoid` provides ergonomic collectors that produce an `AudioBuffer`:

```rust
use prismatoid::{init, Result};
use std::fs;

fn main() -> Result<()> {
    let ctx = init()?;
    let mut backend = ctx.create_best()?;

    // Synthesize text into an in-memory AudioBuffer
    let buffer = backend.synthesize("Welcome to the adventure game!")?;

    println!(
        "Synthesized {} frames, {:.2}s at {} Hz",
        buffer.frames_count(),
        buffer.duration_seconds(),
        buffer.sample_rate
    );

    // Convert directly to WAV container bytes
    let wav_bytes = buffer.to_wav_bytes();
    fs::write("speech.wav", wav_bytes).expect("failed to write WAV file");

    Ok(())
}
```

---

## Custom Rust Backends

You can register user-defined speech or audio backends directly in Rust using the `CustomBackend` trait:

```rust
use prismatoid::{
    init_with_config, BackendFeatures, Config, CustomBackend, Error, RegistryBuilder,
};

#[derive(Default)]
struct GameAudioBackend;

impl CustomBackend for GameAudioBackend {
    fn name(&self) -> &str {
        "GameAudioSynthesizer"
    }

    fn speak(&mut self, text: &str, _interrupt: bool) -> Result<(), Error> {
        println!("[GameAudio] Synthesizing: {text}");
        Ok(())
    }

    fn stop(&mut self) -> Result<(), Error> {
        Ok(())
    }
}

fn main() -> prismatoid::Result<()> {
    let mut builder = RegistryBuilder::new()?;

    let features = BackendFeatures::SUPPORTS_SPEAK | BackendFeatures::SUPPORTS_STOP;
    builder.add_backend::<GameAudioBackend>("GameAudio", 100, features)?;

    let registry = builder.freeze()?;
    let config = Config::builder().registry(registry).build();

    let ctx = init_with_config(config)?;
    let mut backend = ctx.create_best()?;
    backend.speak("Custom backend initialized successfully!", true)?;

    Ok(())
}
```

---

## Logging Diagnostics

Prism includes an asynchronous diagnostic logging pipeline. You can capture log events with a closure or bridge them to the standard `log` crate:

```rust
use prismatoid::logging::{init_log_bridge, set_log_level, LogLevel};

fn main() {
    set_log_level(LogLevel::Debug);

    #[cfg(feature = "log")]
    init_log_bridge();
}
```

---

## Running Tests

To run the complete test suite using the built-in mock backend:

```bash
cargo test --all-features
```

To run clippy lint verification:

```bash
cargo clippy --all-targets --all-features -- -D warnings
```

---

## License

This project is licensed under the [Mozilla Public License Version 2.0](https://www.mozilla.org/en-US/MPL/2.0/) (MPL-2.0).
