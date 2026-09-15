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
fn test_custom_backend_all_methods() {
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
    #[cfg(feature = "mock")]
    {
        let loaded1 = builder
            .add_library("dummy_plugin.dll", None)
            .expect("add_library with None priority failed");
        assert_eq!(loaded1, 1);

        let loaded2 = builder
            .add_library(std::path::Path::new("dummy_plugin.dll"), Some(50))
            .expect("add_library with Some priority failed");
        assert_eq!(loaded2, 1);
    }
    #[cfg(not(feature = "mock"))]
    {
        assert!(builder.add_library("nonexistent_plugin.dll", None).is_err());
        assert!(builder
            .add_library(std::path::Path::new("nonexistent_plugin.dll"), Some(50))
            .is_err());
    }

    let registry = builder.freeze().expect("failed to freeze");
    let config = Config::builder().registry(registry).build();
    let ctx = init_with_config(config).expect("init failed");
    assert!(ctx.count() > 0);

    let mut b = ctx
        .create_backend(custom_id)
        .expect("create custom backend failed");

    // Invoke each of the 11 methods through the backend API
    b.output("custom output", false).expect("output failed");
    b.braille("custom braille").expect("braille failed");
    b.refresh_voices().expect("refresh_voices failed");
    let vcount = b.voices_count().expect("voices_count failed");
    assert!(vcount > 0);
    let vname = b.voice_name(0).expect("voice_name failed");
    assert!(!vname.is_empty());
    let vlang = b.voice_language(0).expect("voice_language failed");
    assert!(!vlang.is_empty());
    b.set_voice(0).expect("set_voice failed");
    let cur_v = b.voice().expect("voice failed");
    assert_eq!(cur_v, 0);

    let ch = b.channels().expect("channels failed");
    assert!(ch > 0);
    let sr = b.sample_rate().expect("sample_rate failed");
    assert!(sr > 0);
    let bd = b.bit_depth().expect("bit_depth failed");
    assert!(bd > 0);

    let mut samples_received = Vec::new();
    b.speak_to_memory("synthesize to memory", |samples, channels, rate| {
        assert!(channels > 0);
        assert!(rate > 0);
        samples_received.extend_from_slice(samples);
    })
    .expect("speak_to_memory failed");
    assert!(!samples_received.is_empty());

    let buf = b.synthesize("synth test").expect("synthesize failed");
    assert!(!buf.samples.is_empty());

    let stream = b
        .synthesize_stream("stream test")
        .expect("synthesize_stream failed");
    assert!(!stream.is_empty());
    let chunks: Vec<_> = stream.collect();
    assert!(!chunks.is_empty());
}

#[test]
fn test_custom_backend_braille() {
    use std::sync::Mutex;

    let braille_buffer = Arc::new(Mutex::new(String::new()));
    let braille_buffer_clone = Arc::clone(&braille_buffer);

    #[derive(Default)]
    struct BrailleBackend {
        buffer: Arc<Mutex<String>>,
    }

    impl CustomBackend for BrailleBackend {
        fn name(&self) -> &str {
            "BrailleBackend"
        }

        fn braille(&mut self, text: &str) -> Result<(), Error> {
            let mut guard = self.buffer.lock().unwrap();
            guard.push_str(text);
            Ok(())
        }
    }

    let mut builder = RegistryBuilder::new().expect("failed to create RegistryBuilder");
    let custom_id = builder
        .add_backend_with_factory(
            "BrailleBackend",
            100,
            BackendFeatures::SUPPORTS_BRAILLE,
            move || BrailleBackend {
                buffer: Arc::clone(&braille_buffer_clone),
            },
        )
        .expect("failed to add braille backend");

    let registry = builder.freeze().expect("failed to freeze");
    let config = Config::builder().registry(registry).build();
    let ctx = init_with_config(config).expect("init failed");

    let mut backend = ctx
        .create_backend(custom_id)
        .expect("failed to create backend");
    backend
        .braille("test braille output")
        .expect("braille failed");

    #[cfg(not(feature = "mock"))]
    assert_eq!(*braille_buffer.lock().unwrap(), "test braille output");
}

#[test]
fn test_error_nul_mapping() {
    let nul_err = Error::NulError("interior nul".into());
    let sys_err: prismatoid_sys::PrismError = nul_err.into();
    assert_eq!(sys_err, prismatoid_sys::PrismError::InvalidParam);
}
