// SPDX-License-Identifier: MPL-2.0

use crate::backend_id::BackendId;
use crate::custom::Registry;
use std::ffi::{c_char, c_void, CStr};

/// Type alias for the dynamic backend availability closure.
pub type AvailabilityCallback = Box<dyn FnMut(BackendId, &str, bool) + Send + 'static>;

/// Configuration settings for initializing a Prism context.
pub struct Config {
    pub poll_interval_ms: u32,
    pub debounce_samples: u32,
    pub backoff_max_ms: u32,
    pub auto_power_manage: bool,
    pub(crate) registry: Option<Registry>,
    pub(crate) availability_callback: Option<AvailabilityCallback>,
}

impl Default for Config {
    fn default() -> Self {
        let raw = unsafe { prismatoid_sys::prism_config_init() };
        Self {
            poll_interval_ms: raw.availability_poll_interval_ms,
            debounce_samples: raw.availability_debounce_samples,
            backoff_max_ms: raw.availability_backoff_max_ms,
            auto_power_manage: raw.availability_auto_power_manage,
            registry: None,
            availability_callback: None,
        }
    }
}

impl Config {
    /// Constructs a builder for fine-grained configuration.
    pub fn builder() -> ConfigBuilder {
        ConfigBuilder::new()
    }
}

/// Builder for constructing a `Config`.
pub struct ConfigBuilder {
    config: Config,
}

impl Default for ConfigBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl ConfigBuilder {
    /// Creates a new `ConfigBuilder` with default options.
    pub fn new() -> Self {
        Self {
            config: Config::default(),
        }
    }

    /// Sets a custom backend registry.
    pub fn registry(mut self, registry: Registry) -> Self {
        self.config.registry = Some(registry);
        self
    }

    /// Sets the interval in milliseconds for polling backend availability.
    pub fn poll_interval_ms(mut self, interval_ms: u32) -> Self {
        self.config.poll_interval_ms = interval_ms;
        self
    }

    /// Sets the number of consecutive samples required before signaling a state change.
    pub fn debounce_samples(mut self, samples: u32) -> Self {
        self.config.debounce_samples = samples;
        self
    }

    /// Sets the maximum backoff interval in milliseconds during idle periods.
    pub fn backoff_max_ms(mut self, max_ms: u32) -> Self {
        self.config.backoff_max_ms = max_ms;
        self
    }

    /// Sets whether Prism should manage polling based on OS power state transitions.
    pub fn auto_power_manage(mut self, enable: bool) -> Self {
        self.config.auto_power_manage = enable;
        self
    }

    /// Sets an asynchronous callback invoked when a backend's availability changes at runtime.
    pub fn on_availability<F>(mut self, callback: F) -> Self
    where
        F: FnMut(BackendId, &str, bool) + Send + 'static,
    {
        self.config.availability_callback = Some(Box::new(callback));
        self
    }

    /// Finalizes and returns the `Config`.
    pub fn build(self) -> Config {
        self.config
    }
}

pub(crate) unsafe extern "C" fn availability_trampoline(
    userdata: *mut c_void,
    backend: prismatoid_sys::PrismBackendId,
    name: *const c_char,
    available: bool,
) {
    if userdata.is_null() {
        return;
    }
    let cb = &mut *(userdata as *mut AvailabilityCallback);
    let name_str = if name.is_null() {
        ""
    } else {
        CStr::from_ptr(name).to_str().unwrap_or("")
    };
    cb(BackendId::from_raw(backend), name_str, available);
}
