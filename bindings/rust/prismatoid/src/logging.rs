// SPDX-License-Identifier: MPL-2.0

//! Logging subsystem for Prism.
//!
//! Prism features an internal asynchronous logging subsystem. Log handlers receive
//! diagnostic messages from Prism and its backends.

use crate::error::Result;
use prismatoid_sys as sys;
use std::ffi::{c_char, c_void, CStr, CString};
use std::sync::Mutex;

/// Severity levels for Prism log messages.
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
#[repr(u32)]
pub enum LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    None = 5,
}

impl From<sys::PrismLogLevel> for LogLevel {
    fn from(level: sys::PrismLogLevel) -> Self {
        match level {
            sys::PrismLogLevel::Trace => LogLevel::Trace,
            sys::PrismLogLevel::Debug => LogLevel::Debug,
            sys::PrismLogLevel::Info => LogLevel::Info,
            sys::PrismLogLevel::Warn => LogLevel::Warn,
            sys::PrismLogLevel::Error => LogLevel::Error,
            sys::PrismLogLevel::None => LogLevel::None,
        }
    }
}

impl From<LogLevel> for sys::PrismLogLevel {
    fn from(level: LogLevel) -> Self {
        match level {
            LogLevel::Trace => sys::PrismLogLevel::Trace,
            LogLevel::Debug => sys::PrismLogLevel::Debug,
            LogLevel::Info => sys::PrismLogLevel::Info,
            LogLevel::Warn => sys::PrismLogLevel::Warn,
            LogLevel::Error => sys::PrismLogLevel::Error,
            LogLevel::None => sys::PrismLogLevel::None,
        }
    }
}

type LogCallback = Box<dyn Fn(LogLevel, &str, &str) + Send + Sync + 'static>;
static LOG_HANDLER: Mutex<Option<LogCallback>> = Mutex::new(None);

unsafe extern "C" fn log_trampoline(
    _userdata: *mut c_void,
    level: sys::PrismLogLevel,
    source: *const c_char,
    message: *const c_char,
) {
    let source_str = if source.is_null() {
        "prism"
    } else {
        CStr::from_ptr(source).to_str().unwrap_or("prism")
    };
    let message_str = if message.is_null() {
        ""
    } else {
        CStr::from_ptr(message).to_str().unwrap_or("")
    };
    let rust_level = LogLevel::from(level);

    if let Ok(guard) = LOG_HANDLER.lock() {
        if let Some(ref cb) = *guard {
            cb(rust_level, source_str, message_str);
        }
    }
}

/// Sets a custom log handler callback for Prism diagnostics.
pub fn set_log_handler<F>(handler: F)
where
    F: Fn(LogLevel, &str, &str) + Send + Sync + 'static,
{
    {
        let mut guard = LOG_HANDLER.lock().expect("logging mutex poisoned");
        *guard = Some(Box::new(handler));
    }
    unsafe {
        sys::prism_set_log_handler(sys::PrismLogHandler {
            fn_callback: Some(log_trampoline),
            userdata: std::ptr::null_mut(),
        });
    }
}

/// Removes the active log handler.
pub fn remove_log_handler() {
    {
        let mut guard = LOG_HANDLER.lock().expect("logging mutex poisoned");
        *guard = None;
    }
    unsafe {
        sys::prism_set_log_handler(sys::PrismLogHandler {
            fn_callback: None,
            userdata: std::ptr::null_mut(),
        });
    }
}

/// Sets the active log filtering level. Returns the previous log level.
pub fn set_log_level(level: LogLevel) -> LogLevel {
    let prev = unsafe { sys::prism_set_log_level(level.into()) };
    prev.into()
}

/// Sends a log message through Prism's logging pipeline.
pub fn log(level: LogLevel, source: &str, message: &str) -> Result<()> {
    let c_source = CString::new(source)?;
    let c_message = CString::new(message)?;
    unsafe {
        sys::prism_log(level.into(), c_source.as_ptr(), c_message.as_ptr());
    }
    Ok(())
}

/// Flushes pending messages in Prism's log queue.
pub fn flush_log() {
    unsafe {
        sys::prism_log_flush();
    }
}

/// Drains and shuts down the Prism background logging thread.
pub fn shutdown_log() {
    unsafe {
        sys::prism_log_shutdown();
    }
}

#[cfg(feature = "log")]
/// Bridges Prism log messages to the standard Rust `log` facade.
pub fn init_log_bridge() {
    set_log_handler(|level, source, message| {
        let target = format!("prism::{source}");
        match level {
            LogLevel::Trace => ::log::trace!(target: &target, "{message}"),
            LogLevel::Debug => ::log::debug!(target: &target, "{message}"),
            LogLevel::Info => ::log::info!(target: &target, "{message}"),
            LogLevel::Warn => ::log::warn!(target: &target, "{message}"),
            LogLevel::Error => ::log::error!(target: &target, "{message}"),
            LogLevel::None => {}
        }
    });
}
