// SPDX-License-Identifier: MPL-2.0

use prismatoid::logging::{
    flush_log, log, remove_log_handler, set_log_handler, set_log_level, shutdown_log, LogLevel,
};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

static TEST_MUTEX: Mutex<()> = Mutex::new(());

#[test]
fn test_logging_level_and_handler() {
    let _guard = TEST_MUTEX.lock().unwrap();
    let received = Arc::new(AtomicBool::new(false));
    let received_clone = Arc::clone(&received);

    set_log_handler(move |level, source, message| {
        if source == "test_source" && message == "test_message" && level == LogLevel::Info {
            received_clone.store(true, Ordering::SeqCst);
        }
    });

    let _prev = set_log_level(LogLevel::Debug);
    log(LogLevel::Info, "test_source", "test_message").expect("log call failed");
    flush_log();

    assert!(received.load(Ordering::SeqCst));

    remove_log_handler();
    shutdown_log();
}

#[test]
fn test_logging_panic_safety() {
    let _guard = TEST_MUTEX.lock().unwrap();
    set_log_handler(|_, _, _| {
        panic!("intentional log handler panic");
    });
    // log calls log_trampoline, which wraps in catch_unwind and must not crash
    let _ = log(LogLevel::Error, "panic_test", "will trigger panic");
    flush_log();
    remove_log_handler();
}

#[cfg(feature = "log")]
#[test]
fn test_log_bridge() {
    let _guard = TEST_MUTEX.lock().unwrap();
    prismatoid::logging::init_log_bridge();
    let _ = log(LogLevel::Warn, "bridge_test", "warning message");
    flush_log();
    remove_log_handler();
}

#[cfg(feature = "tracing")]
#[test]
fn test_tracing_bridge() {
    let _guard = TEST_MUTEX.lock().unwrap();
    prismatoid::logging::init_tracing_bridge();
    let _ = log(LogLevel::Warn, "tracing_test", "warning tracing message");
    flush_log();
    remove_log_handler();
}
