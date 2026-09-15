// SPDX-License-Identifier: MPL-2.0

use prismatoid::{
    init_with_config, BackendFeatures, Config, CustomBackend, Error, RegistryBuilder,
};
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

struct MockSynthesizer {
    spoken: Arc<AtomicBool>,
}

impl Default for MockSynthesizer {
    fn default() -> Self {
        Self {
            spoken: Arc::new(AtomicBool::new(false)),
        }
    }
}

impl CustomBackend for MockSynthesizer {
    fn name(&self) -> &str {
        "MockSynthesizer"
    }

    fn speak(&mut self, _text: &str, _interrupt: bool) -> Result<(), Error> {
        self.spoken.store(true, Ordering::SeqCst);
        Ok(())
    }

    fn is_speaking(&self) -> Result<bool, Error> {
        Ok(self.spoken.load(Ordering::SeqCst))
    }

    fn stop(&mut self) -> Result<(), Error> {
        self.spoken.store(false, Ordering::SeqCst);
        Ok(())
    }
}

#[test]
fn test_custom_backend_registration() {
    let mut builder = RegistryBuilder::new().expect("failed to create RegistryBuilder");

    let features = BackendFeatures::SUPPORTS_SPEAK | BackendFeatures::SUPPORTS_STOP;
    let custom_id = builder
        .add_backend::<MockSynthesizer>("MockSynthesizer", 100, features)
        .expect("failed to add custom backend");

    assert_ne!(custom_id.0, 0);

    let registry = builder.freeze().expect("failed to freeze registry");
    let config = Config::builder().registry(registry).build();

    let ctx = init_with_config(config).expect("failed to init context with custom registry");
    assert!(ctx.count() > 0);
}
