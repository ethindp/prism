// SPDX-License-Identifier: MPL-2.0

//! # prismatoid
//!
//! Idiomatic, memory-safe Rust bindings for **Prism** (Platform-agnostic Reader Interface for Speech and Messages).
//!
//! Prism provides a unified, thread-safe API for speech synthesis and screen reader output
//! across Windows, macOS, Linux, Android, and WebAssembly.
//!
//! ## Overview
//!
//! - **Context & Lifecycle:** [`Context`] initializes and manages Prism's background threads,
//!   backend registry, and availability polling.
//! - **Speech & Audio:** [`Backend`] provides high-level speech commands (`speak`, `stop`, `pause`,
//!   `resume`), voice parameter controls (`volume`, `rate`, `pitch`), and in-memory synthesis
//!   via [`AudioBuffer`].
//! - **Rich Ergonomics:** In-memory speech synthesis can be collected directly into an [`AudioBuffer`],
//!   which provides zero-copy sample slices, PCM conversion, and automated WAV container export.
//! - **Extensibility:** Implement the [`CustomBackend`] trait to register custom speech synthesizers
//!   or game-specific audio pipelines into Prism via [`RegistryBuilder`].
//! - **Logging:** Diagnostic messages from Prism can be routed to a custom closure or bridged
//!   directly to the standard Rust `log` crate.
//!
//! ## Quick Example
//!
//! ```no_run
//! use prismatoid::{init, Result};
//!
//! fn main() -> Result<()> {
//!     // Initialize Prism context
//!     let ctx = init()?;
//!
//!     // Obtain the highest-priority available backend
//!     let mut backend = ctx.create_best()?;
//!
//!     // Speak some text
//!     backend.speak("Hello from Prism and Rust!", true)?;
//!
//!     Ok(())
//! }
//! ```

pub mod audio;
pub mod backend;
pub mod backend_id;
pub mod config;
pub mod context;
pub mod custom;
pub mod error;
pub mod features;
pub mod logging;
pub mod voice;

pub use audio::{AudioBuffer, AudioChunk, AudioFormat, ChunkIterator};
pub use backend::Backend;
pub use backend_id::BackendId;
pub use config::{AvailabilityCallback, Config, ConfigBuilder};
pub use context::Context;
pub use custom::{CustomBackend, Registry, RegistryBuilder};
pub use error::{Error, Result};
pub use features::BackendFeatures;
pub use logging::LogLevel;
pub use voice::VoiceInfo;

/// Raw C bindings to the underlying Prism library.
pub use prismatoid_sys as sys;

/// Initializes Prism with default configuration.
pub fn init() -> Result<Context> {
    Context::new()
}

/// Initializes Prism with the specified configuration.
pub fn init_with_config(config: Config) -> Result<Context> {
    Context::with_config(config)
}

/// Returns the Prism library version packed into a 32-bit integer (`(major << 16) | (minor << 8) | patch`).
pub fn version() -> u32 {
    unsafe { sys::prism_version() }
}

/// Returns the Prism library semantic version string (e.g. `"0.18.2"`).
pub fn version_string() -> &'static str {
    unsafe {
        let ptr = sys::prism_version_string();
        if ptr.is_null() {
            "unknown"
        } else {
            std::ffi::CStr::from_ptr(ptr).to_str().unwrap_or("unknown")
        }
    }
}
