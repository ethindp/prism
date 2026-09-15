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
fn test_is_supported_and_current_voice() {
    let ctx = init().expect("init failed");
    let backend = ctx.create_best().expect("create backend failed");

    assert!(backend.is_supported());

    let voice = backend.current_voice().expect("current_voice failed");
    assert!(voice.is_some());
    let v = voice.unwrap();
    assert_eq!(v.id, 0);
    assert_eq!(v.name, "David");
    assert_eq!(v.language, "en-US");
}

#[test]
fn test_synthesize_stream() {
    let ctx = init().expect("init failed");
    let mut backend = ctx.create_best().expect("create backend failed");

    let stream = backend
        .synthesize_stream("Streaming test text")
        .expect("synthesize_stream failed");
    assert_eq!(stream.len(), 2);

    let chunks: Vec<_> = stream.collect();
    assert_eq!(chunks.len(), 2);
    assert_eq!(chunks[0].samples.len(), 100);
    assert_eq!(chunks[1].samples.len(), 100);
    assert_eq!(chunks[0].channels, 2);
    assert_eq!(chunks[0].sample_rate, 44100);
    assert!(chunks[0].duration_seconds() > 0.0);
    assert_eq!(chunks[0].frames_count(), 50);
}

#[test]
fn test_empty_text_validation() {
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
