// SPDX-License-Identifier: MPL-2.0

//! Basic speech output example using Prism and Rust.

use prismatoid::{init, Result};

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

    println!("Speaking message...");
    backend.speak(
        "Hello from Prism and Rust! Accessible audio gaming engine integration.",
        true,
    )?;

    println!("Speaking... Press Enter to exit.");
    let mut line = String::new();
    let _ = std::io::stdin().read_line(&mut line);

    Ok(())
}
