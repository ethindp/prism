// SPDX-License-Identifier: MPL-2.0

use prismatoid::{init, BackendFeatures, Error};

#[test]
fn test_backend_features() {
    let ctx = init().expect("init failed");
    let backend = ctx.create_best().expect("create backend failed");

    let features = backend.features().expect("get features failed");
    assert!(features.contains(BackendFeatures::SUPPORTS_SPEAK));
    assert!(features.contains(BackendFeatures::SUPPORTS_SPEAK_TO_MEMORY));
}

#[test]
fn test_speech_lifecycle() {
    let ctx = init().expect("init failed");
    let mut backend = ctx.create_best().expect("create backend failed");

    backend.speak("Test speech", true).expect("speak failed");
    let speaking = backend.is_speaking().expect("is_speaking failed");
    assert!(speaking);

    backend.pause().expect("pause failed");
    backend.resume().expect("resume failed");
    backend.stop().expect("stop failed");

    let speaking_after_stop = backend.is_speaking().expect("is_speaking failed");
    assert!(!speaking_after_stop);
}

#[test]
fn test_voice_parameters() {
    let ctx = init().expect("init failed");
    let mut backend = ctx.create_best().expect("create backend failed");

    backend.set_volume(0.75).expect("set_volume failed");
    let vol = backend.volume().expect("volume failed");
    assert!((vol - 0.75).abs() < 1e-4);

    backend.set_rate(0.8).expect("set_rate failed");
    let r = backend.rate().expect("rate failed");
    assert!((r - 0.8).abs() < 1e-4);

    backend.set_pitch(0.7).expect("set_pitch failed");
    let p = backend.pitch().expect("pitch failed");
    assert!((p - 0.7).abs() < 1e-4);
}

#[test]
fn test_voices_enumeration() {
    let ctx = init().expect("init failed");
    let mut backend = ctx.create_best().expect("create backend failed");

    let count = backend.voices_count().expect("voices_count failed");
    assert!(count > 0);

    let voices = backend.voices().expect("voices failed");
    assert_eq!(voices.len(), count);

    let v0 = &voices[0];
    assert!(!v0.name.is_empty());

    let found = backend.find_voice(&v0.name).expect("find_voice failed");
    assert!(found.is_some());
    assert_eq!(found.unwrap().id, v0.id);

    backend.set_voice(0).expect("set_voice failed");
    let current_id = backend.voice().expect("voice failed");
    assert_eq!(current_id, 0);
}

#[test]
fn test_audio_format_and_synthesis() {
    let ctx = init().expect("init failed");
    let mut backend = ctx.create_best().expect("create backend failed");

    let fmt = backend.audio_format().expect("audio_format failed");
    assert!(fmt.channels > 0);
    assert!(fmt.sample_rate > 0);
    assert!(fmt.bit_depth > 0);

    let buffer = backend
        .synthesize("Synthesize this to buffer")
        .expect("synthesize failed");

    assert!(!buffer.samples.is_empty());
    assert_eq!(buffer.channels, fmt.channels);
    assert_eq!(buffer.sample_rate, fmt.sample_rate);
    assert!(buffer.duration_seconds() > 0.0);

    let pcm16 = buffer.to_i16_pcm();
    assert_eq!(pcm16.len(), buffer.samples.len());

    let wav = buffer.to_wav_bytes();
    assert!(wav.len() > 44);
    assert_eq!(&wav[0..4], b"RIFF");
    assert_eq!(&wav[8..12], b"WAVE");
    assert_eq!(&wav[12..16], b"fmt ");
}

#[test]
fn test_backend_is_supported() {
    let ctx = init().expect("init failed");
    let backend = ctx.create_best().expect("create backend failed");
    let supported: bool = backend.is_supported();
    assert!(supported);
}

#[test]
fn test_current_voice_ok() {
    let ctx = init().expect("init failed");
    let backend = ctx.create_best().expect("create backend failed");

    // 1. Returns Ok(Some(voice)) on voice-enabled backends
    let voice = backend.current_voice().expect("current_voice failed");
    assert!(voice.is_some());
    let v = voice.unwrap();
    assert!(!v.name.is_empty());
    assert!(!v.language.is_empty());

    // 2. Returns Ok(None) when NoVoices or NotImplemented is encountered
    #[cfg(feature = "mock")]
    {
        let mut backend = backend;
        // Mock backend maps u32::MAX to NoVoices
        backend
            .set_voice(u32::MAX as usize)
            .expect("set_voice failed");
        let no_voice = backend
            .current_voice()
            .expect("current_voice on NoVoices failed");
        assert!(no_voice.is_none());
        // Restore voice
        backend.set_voice(0).expect("restore voice failed");
    }
    #[cfg(not(feature = "mock"))]
    {
        #[derive(Default)]
        struct NoVoiceBackend;
        impl prismatoid::CustomBackend for NoVoiceBackend {
            fn name(&self) -> &str {
                "NoVoiceBackend"
            }
            fn voice(&self) -> Result<usize, Error> {
                Err(Error::NoVoices)
            }
        }
        let mut builder = prismatoid::RegistryBuilder::new().expect("builder failed");
        let id = builder
            .add_backend::<NoVoiceBackend>("NoVoiceBackend", 200, BackendFeatures::empty())
            .expect("add_backend failed");
        let reg = builder.freeze().expect("freeze failed");
        let custom_ctx =
            prismatoid::init_with_config(prismatoid::Config::builder().registry(reg).build())
                .expect("init failed");
        let no_v_backend = custom_ctx.create_backend(id).expect("create failed");
        let res = no_v_backend
            .current_voice()
            .expect("current_voice on NoVoices failed");
        assert!(res.is_none());
    }
}

#[test]
fn test_synthesize_stream() {
    let ctx = init().expect("init failed");
    let mut backend = ctx.create_best().expect("create backend failed");

    let stream = backend
        .synthesize_stream("hello")
        .expect("synthesize_stream failed");
    assert!(!stream.is_empty());

    let chunks: Vec<_> = stream.collect();
    assert!(!chunks.is_empty());
    let c0 = &chunks[0];
    assert!(!c0.samples.is_empty());
    assert!(c0.channels > 0);
    assert!(c0.sample_rate > 0);
    assert!(c0.duration_seconds() > 0.0);
    assert_eq!(c0.frames_count(), c0.samples.len() / c0.channels);

    let pcm16 = c0.to_i16_pcm();
    assert_eq!(pcm16.len(), c0.samples.len());
}

#[test]
fn test_empty_input_validation() {
    let ctx = init().expect("init failed");
    let mut backend = ctx.create_best().expect("create backend failed");

    assert!(matches!(
        backend.speak("", true),
        Err(Error::InvalidParam(_))
    ));
    assert!(matches!(backend.braille(""), Err(Error::InvalidParam(_))));
    assert!(matches!(
        backend.output("", false),
        Err(Error::InvalidParam(_))
    ));
    assert!(matches!(
        backend.speak_to_memory("", |_, _, _| {}),
        Err(Error::InvalidParam(_))
    ));
    assert!(matches!(
        backend.synthesize(""),
        Err(Error::InvalidParam(_))
    ));
    assert!(matches!(
        backend.synthesize_stream(""),
        Err(Error::InvalidParam(_))
    ));
}

#[test]
fn test_nul_byte_rejection() {
    let ctx = init().expect("init failed");
    let mut backend = ctx.create_best().expect("create backend failed");

    assert!(matches!(
        backend.speak("hello\0world", false),
        Err(Error::NulError(_))
    ));
    assert!(matches!(
        backend.braille("hello\0world"),
        Err(Error::NulError(_))
    ));
    assert!(matches!(
        backend.output("hello\0world", false),
        Err(Error::NulError(_))
    ));
    assert!(matches!(
        backend.speak_to_memory("hello\0world", |_, _, _| {}),
        Err(Error::NulError(_))
    ));
    assert!(matches!(
        backend.synthesize("hello\0world"),
        Err(Error::NulError(_))
    ));
    assert!(matches!(
        backend.synthesize_stream("hello\0world"),
        Err(Error::NulError(_))
    ));
}
