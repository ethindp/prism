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

#[derive(Default)]
struct FullCustomBackend {
    voice_idx: usize,
}

impl CustomBackend for FullCustomBackend {
    fn name(&self) -> &str {
        "FullCustomBackend"
    }

    fn speak_to_memory(
        &mut self,
        text: &str,
        emit: &mut dyn FnMut(&[f32], usize, usize),
    ) -> Result<(), Error> {
        let dummy = vec![0.5f32; 100];
        emit(&dummy, 2, 48000);
        assert!(!text.is_empty());
        Ok(())
    }

    fn braille(&mut self, text: &str) -> Result<(), Error> {
        assert!(!text.is_empty());
        Ok(())
    }

    fn output(&mut self, text: &str, _interrupt: bool) -> Result<(), Error> {
        self.braille(text)
    }

    fn refresh_voices(&mut self) -> Result<(), Error> {
        Ok(())
    }

    fn count_voices(&self) -> Result<usize, Error> {
        Ok(1)
    }

    fn voice_name(&self, _id: usize) -> Result<String, Error> {
        Ok("CustomVoice".to_string())
    }

    fn voice_language(&self, _id: usize) -> Result<String, Error> {
        Ok("en-GB".to_string())
    }

    fn set_voice(&mut self, id: usize) -> Result<(), Error> {
        self.voice_idx = id;
        Ok(())
    }

    fn voice(&self) -> Result<usize, Error> {
        Ok(self.voice_idx)
    }

    fn channels(&self) -> Result<usize, Error> {
        Ok(2)
    }

    fn sample_rate(&self) -> Result<usize, Error> {
        Ok(48000)
    }

    fn bit_depth(&self) -> Result<usize, Error> {
        Ok(32)
    }
}

#[test]
fn test_full_custom_backend_methods() {
    let mut builder = RegistryBuilder::new().expect("failed to create RegistryBuilder");

    let features = BackendFeatures::SUPPORTS_SPEAK_TO_MEMORY
        | BackendFeatures::SUPPORTS_BRAILLE
        | BackendFeatures::SUPPORTS_OUTPUT
        | BackendFeatures::SUPPORTS_REFRESH_VOICES
        | BackendFeatures::SUPPORTS_COUNT_VOICES
        | BackendFeatures::SUPPORTS_GET_VOICE_NAME
        | BackendFeatures::SUPPORTS_GET_VOICE_LANGUAGE
        | BackendFeatures::SUPPORTS_SET_VOICE
        | BackendFeatures::SUPPORTS_GET_VOICE
        | BackendFeatures::SUPPORTS_GET_CHANNELS
        | BackendFeatures::SUPPORTS_GET_SAMPLE_RATE
        | BackendFeatures::SUPPORTS_GET_BIT_DEPTH;

    let custom_id = builder
        .add_backend::<FullCustomBackend>("FullCustomBackend", 100, features)
        .expect("failed to add custom backend");
    assert_ne!(custom_id.0, 0);

    // Test add_library with &str and with Path, Option priority
    let loaded1 = builder
        .add_library("dummy_plugin.dll", None)
        .expect("add_library with None priority failed");
    assert_eq!(loaded1, 1);

    let loaded2 = builder
        .add_library(std::path::Path::new("dummy_plugin.dll"), Some(50))
        .expect("add_library with Some priority failed");
    assert_eq!(loaded2, 1);

    let registry = builder.freeze().expect("failed to freeze");
    let config = Config::builder().registry(registry).build();
    let ctx = init_with_config(config).expect("init failed");
    assert!(ctx.count() > 0);
}

#[test]
fn test_nul_error_mapping() {
    let nul_err = Error::NulError("interior nul".into());
    let sys_err: prismatoid_sys::PrismError = nul_err.into();
    assert_eq!(sys_err, prismatoid_sys::PrismError::InvalidParam);
}
