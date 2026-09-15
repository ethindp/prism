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

    let nvda_backend = ctx
        .create_backend(BackendId::NVDA)
        .expect("failed to create NVDA backend");
    assert!(!nvda_backend.name().unwrap().is_empty());
}
