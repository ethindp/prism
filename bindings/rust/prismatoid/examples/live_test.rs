// SPDX-License-Identifier: MPL-2.0

//! Live test exercising real SAPI 5, OneCore, and screen reader backends with native Prism.

use prismatoid::{init, BackendId, Result};
use std::thread;
use std::time::Duration;

fn main() -> Result<()> {
    let ctx = init()?;
    println!("=== Live Backend Test ===");

    // 1. Test SAPI 5
    println!("\n--- Testing SAPI 5 ---");
    if ctx.backend_exists(BackendId::SAPI) {
        match ctx.create_backend(BackendId::SAPI) {
            Ok(mut sapi) => {
                println!("SAPI backend created: {}", sapi.name()?);
                let count = sapi.voices_count()?;
                println!("SAPI voices installed: {count}");
                for v in sapi.voices()? {
                    println!("  Voice {}: {} [{}]", v.id, v.name, v.language);
                }
                sapi.set_volume(1.0)?;
                sapi.set_rate(1.0)?;
                println!("Speaking via SAPI 5...");
                sapi.speak(
                    "Testing SAPI 5 speech synthesis from Rust prismatoid.",
                    true,
                )?;
                thread::sleep(Duration::from_millis(1500));
                println!("SAPI 5 test completed successfully.");
            }
            Err(e) => println!("Failed to initialize SAPI 5: {e}"),
        }
    } else {
        println!("SAPI 5 is not registered.");
    }

    // 2. Test OneCore (Windows 10/11 natural TTS)
    println!("\n--- Testing Windows OneCore ---");
    if ctx.backend_exists(BackendId::ONE_CORE) {
        match ctx.create_backend(BackendId::ONE_CORE) {
            Ok(mut onecore) => {
                println!("OneCore backend created: {}", onecore.name()?);
                let count = onecore.voices_count()?;
                println!("OneCore voices installed: {count}");
                for v in onecore.voices()? {
                    println!("  Voice {}: {} [{}]", v.id, v.name, v.language);
                }
                println!("Speaking via OneCore...");
                onecore.speak(
                    "Testing Windows OneCore speech synthesis from Rust prismatoid.",
                    true,
                )?;
                thread::sleep(Duration::from_millis(1500));
                println!("OneCore test completed successfully.");
            }
            Err(e) => println!("Failed to initialize OneCore: {e}"),
        }
    }

    // 3. Test Screen Readers (NVDA / JAWS)
    println!("\n--- Testing Screen Readers ---");
    for (name, id) in [("NVDA", BackendId::NVDA), ("JAWS", BackendId::JAWS)] {
        if ctx.backend_exists(id) {
            match ctx.create_backend(id) {
                Ok(mut sr) => {
                    println!("{name} screen reader connected!");
                    sr.speak("Hello from Prism Rust bindings to screen reader.", false)?;
                }
                Err(e) => println!("{name} checked: currently inactive or not running ({e})"),
            }
        }
    }

    println!("\n=== All live backend checks completed! ===");
    Ok(())
}
