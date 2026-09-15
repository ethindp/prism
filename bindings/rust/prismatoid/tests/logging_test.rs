// SPDX-License-Identifier: MPL-2.0

use prismatoid::logging::{
    flush_log, log, remove_log_handler, set_log_handler, set_log_level, shutdown_log, LogLevel,
};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

#[test]
fn test_logging_level_and_handler() {
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

    remove_log_handler();
    shutdown_log();
}

#[cfg(feature = "log")]
#[test]
fn test_log_bridge() {
    prismatoid::logging::init_log_bridge();
    let _ = log(LogLevel::Warn, "bridge_test", "warning message");
    flush_log();
    remove_log_handler();
}
