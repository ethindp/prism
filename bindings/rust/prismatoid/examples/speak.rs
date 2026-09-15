// SPDX-License-Identifier: MPL-2.0

//! Basic speech output example using Prism and Rust.

use prismatoid::{init, BackendFeatures, Result};
use std::thread;
use std::time::Duration;

fn main() -> Result<()> {
    println!("Initializing Prism speech context...");
    let ctx = init()?;

    println!("Available backends ({} total):", ctx.count());
    for backend in ctx.available_backends() {
        println!(" - {} (priority: {})", backend.name, backend.priority);
    }

    println!("\nCreating highest-priority available backend...");
    let mut backend = ctx.create_best()?;
    println!("Selected backend: {}", backend.name()?);

    if backend.supports(BackendFeatures::SUPPORTS_SET_VOLUME) {
        println!("Setting volume to 0.9...");
        let _ = backend.set_volume(0.9);
    }
    if backend.supports(BackendFeatures::SUPPORTS_SET_RATE) {
        println!("Setting rate to 1.0...");
        let _ = backend.set_rate(1.0);
    }

    println!("Speaking message...");
    backend.speak(
        "Hello from Prism and Rust! Accessible audio gaming engine integration.",
        true,
    )?;

    // Allow time for asynchronous speech to complete before exiting
    thread::sleep(Duration::from_millis(1500));

    Ok(())
}
