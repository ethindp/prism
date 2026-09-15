// SPDX-License-Identifier: MPL-2.0

//! SAPI 5 speech output example.

use prismatoid::{init, BackendId, Result};

fn main() -> Result<()> {
    println!("Initializing Prism speech context...");
    let ctx = init()?;

    if !ctx.backend_exists(BackendId::SAPI) {
        eprintln!("SAPI 5 backend is not available on this system.");
        return Ok(());
    }

    let mut backend = ctx.create_backend(BackendId::SAPI)?;
    println!("Selected backend: {}", backend.name()?);

    // List installed SAPI 5 voices
    let count = backend.voices_count()?;
    println!("Installed voices ({count} total):");
    for voice in backend.voices()? {
        println!(" - [{}] {} ({})", voice.id, voice.name, voice.language);
    }

    backend.set_volume(1.0)?;
    backend.set_rate(0.35)?;

    println!("\nSpeaking message via SAPI 5...");
    backend.speak(
        "This is SAPI 5 speaking at a slower rate using Prism and Rust.",
        true,
    )?;

    println!("Speaking... Press Enter to exit.");
    let mut line = String::new();
    let _ = std::io::stdin().read_line(&mut line);

    Ok(())
}
