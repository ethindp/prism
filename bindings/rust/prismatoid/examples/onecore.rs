// SPDX-License-Identifier: MPL-2.0

//! Windows OneCore speech output example.

use prismatoid::{init, BackendId, Result};

fn main() -> Result<()> {
    println!("Initializing Prism speech context...");
    let ctx = init()?;

    if !ctx.backend_exists(BackendId::ONE_CORE) {
        eprintln!("Windows OneCore backend is not available on this system.");
        return Ok(());
    }

    let mut backend = ctx.create_backend(BackendId::ONE_CORE)?;
    println!("Selected backend: {}", backend.name()?);

    // List installed OneCore natural voices
    let count = backend.voices_count()?;
    println!("Installed voices ({count} total):");
    for voice in backend.voices()? {
        println!(" - [{}] {} ({})", voice.id, voice.name, voice.language);
    }

    backend.set_volume(1.0)?;
    backend.set_rate(0.4)?;

    println!("\nSpeaking message via OneCore...");
    backend.speak(
        "This is Windows OneCore speaking with natural voices at a slower rate using Prism and Rust.",
        true,
    )?;

    println!("Speaking... Press Enter to exit.");
    let mut line = String::new();
    let _ = std::io::stdin().read_line(&mut line);

    Ok(())
}
