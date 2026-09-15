// SPDX-License-Identifier: MPL-2.0

//! Windows OneCore speech output example with rate adjustment and speech wait.

use prismatoid::{init, BackendId, Result};
use std::thread;
use std::time::Duration;

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

    // Set volume to 90%
    backend.set_volume(0.9)?;
    println!("Volume set to: {:.2}", backend.volume()?);

    // Slow down speech rate (0.5 is normal, 0.0 is slowest, 1.0 is fastest)
    backend.set_rate(0.3)?;
    println!("Rate slowed down to: {:.2}", backend.rate()?);

    println!("\nSpeaking message via OneCore...");
    backend.speak(
        "This is Windows OneCore speaking with natural voices at a slower rate using Prism and Rust.",
        true,
    )?;

    // Wait briefly for playback to engage, then wait until speaking completes
    thread::sleep(Duration::from_millis(100));
    while backend.is_speaking()? {
        thread::sleep(Duration::from_millis(50));
    }

    println!("OneCore speech finished.");
    Ok(())
}
