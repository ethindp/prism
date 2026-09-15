// SPDX-License-Identifier: MPL-2.0

use prismatoid::{init, init_with_config, version, version_string, BackendId, Config};

#[test]
fn test_version() {
    assert_ne!(version(), 0);
    assert!(!version_string().is_empty());
}

#[test]
fn test_context_lifecycle() {
    let ctx = init().expect("Failed to initialize default Prism context");
    assert!(ctx.count() > 0);
}

#[test]
fn test_config_builder() {
    let config = Config::builder()
        .poll_interval_ms(500)
        .debounce_samples(2)
        .backoff_max_ms(3000)
        .auto_power_manage(false)
        .build();

    let ctx = init_with_config(config).expect("Failed to initialize with config");
    assert!(ctx.count() > 0);
}

#[test]
fn test_registry_queries() {
    let ctx = init().expect("init failed");
    let count = ctx.count();
    assert!(count >= 2);

    let id0 = ctx.id_at(0).expect("backend at index 0 not found");
    assert_ne!(id0, BackendId::INVALID);

    let name0 = ctx.name(id0).expect("failed to get name for backend 0");
    assert!(!name0.is_empty());

    let found_id = ctx.id_for_name(name0);
    assert_eq!(found_id, Some(id0));

    let backends = ctx.available_backends();
    assert_eq!(backends.len(), count);
}

#[test]
fn test_create_and_acquire_backend() {
    let ctx = init().expect("init failed");
    let best = ctx.create_best().expect("failed to create best backend");
    assert!(!best.name().unwrap().is_empty());

    let acquired = ctx.acquire_best().expect("failed to acquire best backend");
    assert!(!acquired.name().unwrap().is_empty());
}

#[test]
fn test_get_backend() {
    let ctx = init().expect("init failed");
    let id0 = ctx.id_at(0).expect("backend 0 not found");

    let b1 = ctx.get_backend(id0);
    assert!(b1.is_some());
    assert!(!b1.unwrap().name().unwrap().is_empty());

    let b2 = ctx.get(id0);
    assert!(b2.is_some());

    let invalid = ctx.get_backend(BackendId::INVALID);
    assert!(invalid.is_none());
}

#[cfg(feature = "mock")]
#[test]
fn test_availability_callback() {
    use std::sync::atomic::{AtomicBool, Ordering};
    use std::sync::Arc;

    let triggered = Arc::new(AtomicBool::new(false));
    let triggered_clone = Arc::clone(&triggered);

    let config = Config::builder()
        .on_availability(move |backend, name, available| {
            if backend.raw() == prismatoid_sys::PRISM_BACKEND_NVDA && name == "NVDA" && available {
                triggered_clone.store(true, Ordering::SeqCst);
            }
        })
        .build();

    let _ctx = init_with_config(config).expect("init failed");

    prismatoid_sys::mock_trigger_availability(prismatoid_sys::PRISM_BACKEND_NVDA, "NVDA", true);

    assert!(triggered.load(Ordering::SeqCst));
}

#[cfg(feature = "mock")]
#[test]
fn test_availability_panic_safety() {
    let config = Config::builder()
        .on_availability(|_, _, _| {
            panic!("intentional availability callback panic");
        })
        .build();

    let _ctx = init_with_config(config).expect("init failed");
    // Must not crash or abort the process because availability_trampoline wraps in catch_unwind
    prismatoid_sys::mock_trigger_availability(prismatoid_sys::PRISM_BACKEND_NVDA, "NVDA", true);
}
