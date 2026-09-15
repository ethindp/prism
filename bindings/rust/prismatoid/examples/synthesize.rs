// SPDX-License-Identifier: MPL-2.0

//! In-memory audio synthesis and WAV export example using Prism and Rust.

use prismatoid::{init, Result};
use std::fs;

fn main() -> Result<()> {
    let ctx = init()?;
    let mut backend = ctx.create_best()?;

    println!("Synthesizing text using backend: {}", backend.name()?);
    let prompt = "Level 1: The Dark Cavern. You hear faint whispers to the northeast.";

    let buffer = backend.synthesize(prompt)?;
    println!(
        "Synthesized {} audio samples across {} channel(s) at {} Hz (duration: {:.2}s)",
        buffer.samples.len(),
        buffer.channels,
        buffer.sample_rate,
        buffer.duration_seconds()
    );

    let wav_bytes = buffer.to_wav_bytes();
    fs::write("synthesized_speech.wav", &wav_bytes).map_err(|e| {
        eprintln!("Failed to write WAV file: {e}");
        prismatoid::Error::SpeakFailure
    })?;

    println!(
        "Successfully exported {} bytes to synthesized_speech.wav",
        wav_bytes.len()
    );
    Ok(())
}
