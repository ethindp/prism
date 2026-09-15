// SPDX-License-Identifier: MPL-2.0

use prismatoid::{init, BackendFeatures};

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

    backend.set_rate(1.5).expect("set_rate failed");
    let r = backend.rate().expect("rate failed");
    assert!((r - 1.5).abs() < 1e-4);

    backend.set_pitch(1.2).expect("set_pitch failed");
    let p = backend.pitch().expect("pitch failed");
    assert!((p - 1.2).abs() < 1e-4);
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
