// SPDX-License-Identifier: MPL-2.0

//! Comprehensive example demonstrating all Prism methods in Rust:
//! - Context configuration, availability, and auto-power support
//! - Backend discovery, enumeration, and acquisition (both cached and fresh)
//! - Feature flags, audio format inspection (channels, rate, bit depth)
//! - Voice enumeration, current voice query, and selection
//! - Volume, rate, and pitch control
//! - Speech, braille, output, pause, resume, and stop
//! - In-memory and streaming audio synthesis (AudioChunk iteration)

use prismatoid::{init, AudioChunk, BackendFeatures, Result};

fn main() -> Result<()> {
    println!("=== Prism Comprehensive Methods Demo ===\n");

    // 1. Context & Auto-Power Support
    let auto_power = prismatoid::Context::is_auto_power_supported();
    println!("Auto power management supported: {auto_power}");

    let ctx = init()?;
    println!("Total registered backends: {}", ctx.count());

    // 2. Backend Discovery & Enumeration
    println!("\nAvailable Backends:");
    for info in ctx.available_backends() {
        println!(
            " - [{:016X}] {:<15} | Priority: {}",
            info.id.raw(),
            info.name,
            info.priority
        );
    }

    // 3. Backend Acquisition (Best available backend)
    let mut backend = ctx.acquire_best()?;
    println!(
        "\nActive Backend: {} | Supported at runtime: {}",
        backend.name()?,
        backend.is_supported()
    );

    // 4. Feature Inspection
    let features = backend.features()?;
    println!("\nSupported Features:");
    if features.contains(BackendFeatures::SUPPORTS_SPEAK) {
        println!(" - Speak");
    }
    if features.contains(BackendFeatures::SUPPORTS_SPEAK_TO_MEMORY) {
        println!(" - Speak to Memory");
    }
    if features.contains(BackendFeatures::SUPPORTS_BRAILLE) {
        println!(" - Braille");
    }
    if features.contains(BackendFeatures::SUPPORTS_OUTPUT) {
        println!(" - Output (Speech + Braille)");
    }
    if features.contains(BackendFeatures::SUPPORTS_IS_SPEAKING) {
        println!(" - Is Speaking Query");
    }
    if features.contains(BackendFeatures::SUPPORTS_STOP) {
        println!(" - Stop");
    }
    if features.contains(BackendFeatures::SUPPORTS_PAUSE) {
        println!(" - Pause");
    }
    if features.contains(BackendFeatures::SUPPORTS_RESUME) {
        println!(" - Resume");
    }
    if features.contains(BackendFeatures::SUPPORTS_SET_VOLUME) {
        println!(" - Volume Control");
    }
    if features.contains(BackendFeatures::SUPPORTS_SET_RATE) {
        println!(" - Rate Control");
    }
    if features.contains(BackendFeatures::SUPPORTS_SET_PITCH) {
        println!(" - Pitch Control");
    }

    // 5. Audio Format Parameters
    if let Ok(format) = backend.audio_format() {
        println!(
            "\nAudio Format: {} channel(s), {} Hz, {}-bit",
            format.channels, format.sample_rate, format.bit_depth
        );
    }

    // 6. Speech Parameters (Volume, Rate, Pitch)
    println!("\nCurrent Parameters:");
    if let Ok(vol) = backend.volume() {
        println!(" - Volume: {:.0}%", vol * 100.0);
    } else {
        println!(" - Volume: Not supported");
    }
    if let Ok(rate) = backend.rate() {
        println!(" - Rate: {:.0}%", rate * 100.0);
    } else {
        println!(" - Rate: Not supported");
    }
    if let Ok(pitch) = backend.pitch() {
        println!(" - Pitch: {:.0}%", pitch * 100.0);
    } else {
        println!(" - Pitch: Not supported");
    }

    // 7. Voice Management
    println!("\nAvailable Voices:");
    match backend.voices() {
        Ok(voices) if !voices.is_empty() => {
            for v in &voices {
                println!(" - [{}] {} ({})", v.id, v.name, v.language);
            }
        }
        _ => println!(" - (Voices not enumerated or managed by screen reader)"),
    }

    if let Ok(Some(current)) = backend.current_voice() {
        println!(
            "Current Active Voice: {} ({})",
            current.name, current.language
        );
    }

    // 8. Streaming Audio Synthesis (if supported)
    if features.contains(BackendFeatures::SUPPORTS_SPEAK_TO_MEMORY) {
        println!("\nSynthesizing speech in-memory (streaming chunks)...");
        match backend.synthesize_stream("This is a test of in-memory streaming audio synthesis.") {
            Ok(stream) => {
                let chunks: Vec<AudioChunk> = stream.collect();
                let total_samples: usize = chunks.iter().map(|c| c.samples.len()).sum();
                println!(
                    "Stream Complete: Received {} chunk(s), {} total samples.",
                    chunks.len(),
                    total_samples
                );
            }
            Err(e) => println!("Memory synthesis error: {e}"),
        }
    }

    // 9. Speech & Braille Output
    let test_message = "Testing all Prism methods from Rust.";
    println!("\nSpeaking message: \"{test_message}\"");
    backend.speak(test_message, true)?;

    if features.contains(BackendFeatures::SUPPORTS_BRAILLE) {
        println!("Sending braille output...");
        let _ = backend.braille("Prism Braille Test");
    }

    // Check if actively speaking
    if let Ok(speaking) = backend.is_speaking() {
        println!("Is currently speaking: {speaking}");
    }

    println!("\nPlayback active. Press Enter to stop and exit...");
    let mut input = String::new();
    let _ = std::io::stdin().read_line(&mut input);

    // Stop playback
    if features.contains(BackendFeatures::SUPPORTS_STOP) {
        let _ = backend.stop();
    }

    println!("Done!");
    Ok(())
}
