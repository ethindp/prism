// SPDX-License-Identifier: MPL-2.0

//! Mock implementation of Prism C API for testing Rust bindings without native library.

use super::*;
use std::ffi::CStr;
use std::sync::atomic::{AtomicBool, AtomicU32, Ordering};
use std::sync::Mutex;

static IS_SPEAKING: AtomicBool = AtomicBool::new(false);
static VOLUME: Mutex<f32> = Mutex::new(1.0);
static RATE: Mutex<f32> = Mutex::new(1.0);
static PITCH: Mutex<f32> = Mutex::new(1.0);
static VOICE_ID: AtomicU32 = AtomicU32::new(0);

type AvailabilityCb = unsafe extern "C" fn(*mut c_void, PrismBackendId, *const c_char, bool);
static AVAILABILITY_CB: Mutex<Vec<(AvailabilityCb, usize)>> = Mutex::new(Vec::new());

struct MockContext {
    _dummy: u8,
    userdata: usize,
}

/// Triggers the registered availability callback in mock tests.
pub fn mock_trigger_availability(backend: PrismBackendId, name: &str, available: bool) {
    let callbacks = {
        let store = AVAILABILITY_CB.lock().unwrap();
        store.clone()
    };
    let c_name = std::ffi::CString::new(name).unwrap_or_default();
    for (cb, userdata_val) in callbacks {
        unsafe {
            cb(
                userdata_val as *mut c_void,
                backend,
                c_name.as_ptr(),
                available,
            );
        }
    }
}

#[no_mangle]
unsafe extern "C" fn prism_config_init() -> PrismConfig {
    PrismConfig {
        version: PRISM_CONFIG_VERSION,
        registry: std::ptr::null_mut(),
        availability_callback: None,
        availability_userdata: std::ptr::null_mut(),
        availability_poll_interval_ms: 1000,
        availability_debounce_samples: 3,
        availability_backoff_max_ms: 5000,
        availability_auto_power_manage: true,
    }
}

#[no_mangle]
unsafe extern "C" fn prism_init(cfg: *mut PrismConfig) -> *mut PrismContext {
    let mut ud_val = 0;
    if !cfg.is_null() {
        if let Some(cb) = (*cfg).availability_callback {
            ud_val = (*cfg).availability_userdata as usize;
            let mut store = AVAILABILITY_CB.lock().unwrap();
            store.push((cb, ud_val));
        }
    }
    Box::into_raw(Box::new(MockContext {
        _dummy: 42,
        userdata: ud_val,
    })) as *mut PrismContext
}

#[no_mangle]
unsafe extern "C" fn prism_shutdown(ctx: *mut PrismContext) {
    if !ctx.is_null() {
        let mock_ctx = Box::from_raw(ctx as *mut MockContext);
        if mock_ctx.userdata != 0 {
            let mut store = AVAILABILITY_CB.lock().unwrap();
            store.retain(|&(_, ud)| ud != mock_ctx.userdata);
        }
    }
}

#[no_mangle]
unsafe extern "C" fn prism_availability_poll_pause(_ctx: *mut PrismContext) {}

#[no_mangle]
unsafe extern "C" fn prism_availability_poll_resume(_ctx: *mut PrismContext) {}

#[no_mangle]
unsafe extern "C" fn prism_availability_auto_power_supported() -> bool {
    true
}

#[no_mangle]
unsafe extern "C" fn prism_registry_count(_ctx: *mut PrismContext) -> usize {
    2
}

#[no_mangle]
unsafe extern "C" fn prism_registry_id_at(_ctx: *mut PrismContext, index: usize) -> PrismBackendId {
    match index {
        0 => PRISM_BACKEND_NVDA,
        1 => PRISM_BACKEND_ONE_CORE,
        _ => PRISM_BACKEND_INVALID,
    }
}

#[no_mangle]
unsafe extern "C" fn prism_registry_id(
    _ctx: *mut PrismContext,
    name: *const c_char,
) -> PrismBackendId {
    if name.is_null() {
        return PRISM_BACKEND_INVALID;
    }
    let cstr = CStr::from_ptr(name);
    match cstr.to_str().unwrap_or("") {
        "NVDA" => PRISM_BACKEND_NVDA,
        "OneCore" => PRISM_BACKEND_ONE_CORE,
        "JAWS" => PRISM_BACKEND_JAWS,
        "ZDSR" => PRISM_BACKEND_ZDSR,
        _ => PRISM_BACKEND_INVALID,
    }
}

#[no_mangle]
unsafe extern "C" fn prism_registry_name(
    _ctx: *mut PrismContext,
    id: PrismBackendId,
) -> *const c_char {
    match id {
        PRISM_BACKEND_NVDA => c"NVDA".as_ptr(),
        PRISM_BACKEND_ONE_CORE => c"OneCore".as_ptr(),
        PRISM_BACKEND_JAWS => c"JAWS".as_ptr(),
        PRISM_BACKEND_ZDSR => c"ZDSR".as_ptr(),
        _ => std::ptr::null(),
    }
}

#[no_mangle]
unsafe extern "C" fn prism_registry_priority(_ctx: *mut PrismContext, id: PrismBackendId) -> i32 {
    match id {
        PRISM_BACKEND_NVDA => 100,
        PRISM_BACKEND_ONE_CORE => 50,
        _ => 0,
    }
}

#[no_mangle]
unsafe extern "C" fn prism_registry_exists(_ctx: *mut PrismContext, id: PrismBackendId) -> bool {
    matches!(
        id,
        PRISM_BACKEND_NVDA | PRISM_BACKEND_ONE_CORE | PRISM_BACKEND_JAWS | PRISM_BACKEND_ZDSR
    )
}

#[no_mangle]
unsafe extern "C" fn prism_registry_get(
    ctx: *mut PrismContext,
    id: PrismBackendId,
) -> *mut PrismBackend {
    if !prism_registry_exists(ctx, id) {
        std::ptr::null_mut()
    } else {
        Box::into_raw(Box::new(100u8)) as *mut PrismBackend
    }
}

#[no_mangle]
unsafe extern "C" fn prism_registry_create(
    _ctx: *mut PrismContext,
    _id: PrismBackendId,
) -> *mut PrismBackend {
    Box::into_raw(Box::new(101u8)) as *mut PrismBackend
}

#[no_mangle]
unsafe extern "C" fn prism_registry_create_best(_ctx: *mut PrismContext) -> *mut PrismBackend {
    Box::into_raw(Box::new(102u8)) as *mut PrismBackend
}

#[no_mangle]
unsafe extern "C" fn prism_registry_acquire(
    _ctx: *mut PrismContext,
    _id: PrismBackendId,
) -> *mut PrismBackend {
    Box::into_raw(Box::new(103u8)) as *mut PrismBackend
}

#[no_mangle]
unsafe extern "C" fn prism_registry_acquire_best(_ctx: *mut PrismContext) -> *mut PrismBackend {
    Box::into_raw(Box::new(104u8)) as *mut PrismBackend
}

#[no_mangle]
unsafe extern "C" fn prism_backend_free(backend: *mut PrismBackend) {
    if !backend.is_null() {
        drop(Box::from_raw(backend as *mut u8));
    }
}

#[no_mangle]
unsafe extern "C" fn prism_backend_name(_backend: *mut PrismBackend) -> *const c_char {
    c"MockBackend".as_ptr()
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_features(_backend: *mut PrismBackend) -> u64 {
    PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME
        | PRISM_BACKEND_SUPPORTS_SPEAK
        | PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY
        | PRISM_BACKEND_SUPPORTS_BRAILLE
        | PRISM_BACKEND_SUPPORTS_OUTPUT
        | PRISM_BACKEND_SUPPORTS_IS_SPEAKING
        | PRISM_BACKEND_SUPPORTS_STOP
        | PRISM_BACKEND_SUPPORTS_PAUSE
        | PRISM_BACKEND_SUPPORTS_RESUME
        | PRISM_BACKEND_SUPPORTS_SET_VOLUME
        | PRISM_BACKEND_SUPPORTS_GET_VOLUME
        | PRISM_BACKEND_SUPPORTS_SET_RATE
        | PRISM_BACKEND_SUPPORTS_GET_RATE
        | PRISM_BACKEND_SUPPORTS_SET_PITCH
        | PRISM_BACKEND_SUPPORTS_GET_PITCH
        | PRISM_BACKEND_SUPPORTS_REFRESH_VOICES
        | PRISM_BACKEND_SUPPORTS_COUNT_VOICES
        | PRISM_BACKEND_SUPPORTS_GET_VOICE_NAME
        | PRISM_BACKEND_SUPPORTS_GET_VOICE_LANGUAGE
        | PRISM_BACKEND_SUPPORTS_GET_VOICE
        | PRISM_BACKEND_SUPPORTS_SET_VOICE
        | PRISM_BACKEND_SUPPORTS_GET_CHANNELS
        | PRISM_BACKEND_SUPPORTS_GET_SAMPLE_RATE
        | PRISM_BACKEND_SUPPORTS_GET_BIT_DEPTH
        | PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY
}

#[no_mangle]
unsafe extern "C" fn prism_backend_initialize(_backend: *mut PrismBackend) -> PrismError {
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_speak(
    _backend: *mut PrismBackend,
    text: *const c_char,
    _interrupt: bool,
) -> PrismError {
    if text.is_null() {
        return PrismError::InvalidParam;
    }
    if CStr::from_ptr(text).to_bytes().is_empty() {
        return PrismError::InvalidParam;
    }
    IS_SPEAKING.store(true, Ordering::SeqCst);
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_speak_to_memory(
    _backend: *mut PrismBackend,
    text: *const c_char,
    callback: Option<PrismAudioCallback>,
    userdata: *mut c_void,
) -> PrismError {
    if text.is_null() {
        return PrismError::InvalidParam;
    }
    if CStr::from_ptr(text).to_bytes().is_empty() {
        return PrismError::InvalidParam;
    }
    if let Some(cb) = callback {
        // Emit 2 chunks of stereo audio at 44100Hz
        let chunk1: Vec<f32> = vec![0.1; 100];
        cb(userdata, chunk1.as_ptr(), 100, 2, 44100);
        let chunk2: Vec<f32> = vec![0.2; 100];
        cb(userdata, chunk2.as_ptr(), 100, 2, 44100);
    }
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_braille(
    _backend: *mut PrismBackend,
    text: *const c_char,
) -> PrismError {
    if text.is_null() {
        return PrismError::InvalidParam;
    }
    if CStr::from_ptr(text).to_bytes().is_empty() {
        return PrismError::InvalidParam;
    }
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_output(
    backend: *mut PrismBackend,
    text: *const c_char,
    interrupt: bool,
) -> PrismError {
    let err = prism_backend_speak(backend, text, interrupt);
    if err != PrismError::Ok {
        return err;
    }
    prism_backend_braille(backend, text)
}

#[no_mangle]
unsafe extern "C" fn prism_backend_stop(_backend: *mut PrismBackend) -> PrismError {
    IS_SPEAKING.store(false, Ordering::SeqCst);
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_pause(_backend: *mut PrismBackend) -> PrismError {
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_resume(_backend: *mut PrismBackend) -> PrismError {
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_is_speaking(
    _backend: *mut PrismBackend,
    out_speaking: *mut bool,
) -> PrismError {
    if out_speaking.is_null() {
        return PrismError::InvalidParam;
    }
    *out_speaking = IS_SPEAKING.load(Ordering::SeqCst);
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_set_volume(
    _backend: *mut PrismBackend,
    volume: f32,
) -> PrismError {
    *VOLUME.lock().unwrap() = volume;
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_volume(
    _backend: *mut PrismBackend,
    out_volume: *mut f32,
) -> PrismError {
    if out_volume.is_null() {
        return PrismError::InvalidParam;
    }
    *out_volume = *VOLUME.lock().unwrap();
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_set_rate(_backend: *mut PrismBackend, rate: f32) -> PrismError {
    *RATE.lock().unwrap() = rate;
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_rate(
    _backend: *mut PrismBackend,
    out_rate: *mut f32,
) -> PrismError {
    if out_rate.is_null() {
        return PrismError::InvalidParam;
    }
    *out_rate = *RATE.lock().unwrap();
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_set_pitch(
    _backend: *mut PrismBackend,
    pitch: f32,
) -> PrismError {
    *PITCH.lock().unwrap() = pitch;
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_pitch(
    _backend: *mut PrismBackend,
    out_pitch: *mut f32,
) -> PrismError {
    if out_pitch.is_null() {
        return PrismError::InvalidParam;
    }
    *out_pitch = *PITCH.lock().unwrap();
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_refresh_voices(_backend: *mut PrismBackend) -> PrismError {
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_count_voices(
    _backend: *mut PrismBackend,
    out_count: *mut usize,
) -> PrismError {
    if out_count.is_null() {
        return PrismError::InvalidParam;
    }
    *out_count = 2;
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_voice_name(
    _backend: *mut PrismBackend,
    voice_id: usize,
    out_name: *mut *const c_char,
) -> PrismError {
    if out_name.is_null() {
        return PrismError::InvalidParam;
    }
    match voice_id {
        0 => *out_name = c"David".as_ptr(),
        1 => *out_name = c"Zira".as_ptr(),
        _ => return PrismError::VoiceNotFound,
    }
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_voice_language(
    _backend: *mut PrismBackend,
    voice_id: usize,
    out_language: *mut *const c_char,
) -> PrismError {
    if out_language.is_null() {
        return PrismError::InvalidParam;
    }
    match voice_id {
        0 => *out_language = c"en-US".as_ptr(),
        1 => *out_language = c"en-US".as_ptr(),
        _ => return PrismError::VoiceNotFound,
    }
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_set_voice(
    _backend: *mut PrismBackend,
    voice_id: usize,
) -> PrismError {
    if voice_id >= 2 {
        return PrismError::VoiceNotFound;
    }
    VOICE_ID.store(voice_id as u32, Ordering::SeqCst);
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_voice(
    _backend: *mut PrismBackend,
    out_voice_id: *mut usize,
) -> PrismError {
    if out_voice_id.is_null() {
        return PrismError::InvalidParam;
    }
    *out_voice_id = VOICE_ID.load(Ordering::SeqCst) as usize;
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_channels(
    _backend: *mut PrismBackend,
    out_channels: *mut usize,
) -> PrismError {
    if out_channels.is_null() {
        return PrismError::InvalidParam;
    }
    *out_channels = 2;
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_sample_rate(
    _backend: *mut PrismBackend,
    out_sample_rate: *mut usize,
) -> PrismError {
    if out_sample_rate.is_null() {
        return PrismError::InvalidParam;
    }
    *out_sample_rate = 44100;
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_backend_get_bit_depth(
    _backend: *mut PrismBackend,
    out_bit_depth: *mut usize,
) -> PrismError {
    if out_bit_depth.is_null() {
        return PrismError::InvalidParam;
    }
    *out_bit_depth = 32;
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_error_string(error: PrismError) -> *const c_char {
    match error {
        PrismError::Ok => c"Operation succeeded".as_ptr(),
        PrismError::NotInitialized => c"Not initialized".as_ptr(),
        PrismError::InvalidParam => c"Invalid parameter".as_ptr(),
        PrismError::NotImplemented => c"Not implemented".as_ptr(),
        PrismError::NoVoices => c"No voices available".as_ptr(),
        PrismError::VoiceNotFound => c"Voice not found".as_ptr(),
        PrismError::SpeakFailure => c"Speak failure".as_ptr(),
        PrismError::MemoryFailure => c"Memory failure".as_ptr(),
        PrismError::RangeOutOfBounds => c"Range out of bounds".as_ptr(),
        PrismError::Internal => c"Internal error".as_ptr(),
        PrismError::NotSpeaking => c"Not speaking".as_ptr(),
        PrismError::NotPaused => c"Not paused".as_ptr(),
        PrismError::AlreadyPaused => c"Already paused".as_ptr(),
        PrismError::InvalidUtf8 => c"Invalid UTF-8".as_ptr(),
        PrismError::InvalidOperation => c"Invalid operation".as_ptr(),
        PrismError::AlreadyInitialized => c"Already initialized".as_ptr(),
        PrismError::BackendNotAvailable => c"Backend not available".as_ptr(),
        PrismError::Unknown => c"Unknown error".as_ptr(),
        PrismError::InvalidAudioFormat => c"Invalid audio format".as_ptr(),
        PrismError::InternalBackendLimitExceeded => c"Internal backend limit exceeded".as_ptr(),
        PrismError::BackendEnteredUndefinedState => c"Backend entered undefined state".as_ptr(),
        PrismError::LibraryLoadFailed => c"Library load failed".as_ptr(),
        PrismError::LibraryInvalid => c"Library invalid".as_ptr(),
        PrismError::IncompatibleAbi => c"Incompatible ABI".as_ptr(),
        PrismError::Count => c"Error count sentinel".as_ptr(),
    }
}

#[no_mangle]
unsafe extern "C" fn prism_registry_builder_new() -> *mut PrismRegistryBuilder {
    Box::into_raw(Box::new(200u8)) as *mut PrismRegistryBuilder
}

#[no_mangle]
unsafe extern "C" fn prism_registry_builder_add_backend(
    _builder: *mut PrismRegistryBuilder,
    _name: *const c_char,
    _priority: i32,
    _features: u64,
    _vtable: *const PrismBackendVTable,
    _userdata: *mut c_void,
    _userdata_free: Option<unsafe extern "C" fn(userdata: *mut c_void)>,
    out_id: *mut PrismBackendId,
) -> PrismError {
    if !out_id.is_null() {
        *out_id = 0x12345678_ABCDEF01;
    }
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_registry_builder_add_library(
    _builder: *mut PrismRegistryBuilder,
    _path: *const c_char,
    _priority_override: i32,
    out_count: *mut usize,
) -> PrismError {
    if !out_count.is_null() {
        *out_count = 1;
    }
    PrismError::Ok
}

#[no_mangle]
unsafe extern "C" fn prism_registry_freeze(
    builder: *mut PrismRegistryBuilder,
) -> *mut PrismRegistry {
    if !builder.is_null() {
        drop(Box::from_raw(builder as *mut u8));
    }
    Box::into_raw(Box::new(42u8)) as *mut PrismRegistry
}

#[no_mangle]
unsafe extern "C" fn prism_registry_builder_free(builder: *mut PrismRegistryBuilder) {
    if !builder.is_null() {
        drop(Box::from_raw(builder as *mut u8));
    }
}

#[no_mangle]
unsafe extern "C" fn prism_registry_retain(registry: *mut PrismRegistry) -> *mut PrismRegistry {
    registry
}

#[no_mangle]
unsafe extern "C" fn prism_registry_release(registry: *mut PrismRegistry) {
    if !registry.is_null() {
        drop(Box::from_raw(registry as *mut u8));
    }
}

static MOCK_LOG_HANDLER: Mutex<Option<(PrismLogCallback, usize)>> = Mutex::new(None);

#[no_mangle]
unsafe extern "C" fn prism_set_log_handler(handler: PrismLogHandler) -> PrismLogHandler {
    let mut store = MOCK_LOG_HANDLER.lock().unwrap();
    let prev = match *store {
        Some((cb, ud)) => PrismLogHandler {
            fn_callback: Some(cb),
            userdata: ud as *mut c_void,
        },
        None => PrismLogHandler {
            fn_callback: None,
            userdata: std::ptr::null_mut(),
        },
    };
    if let Some(cb) = handler.fn_callback {
        *store = Some((cb, handler.userdata as usize));
    } else {
        *store = None;
    }
    prev
}

#[no_mangle]
unsafe extern "C" fn prism_set_log_level(level: PrismLogLevel) -> PrismLogLevel {
    level
}

#[no_mangle]
unsafe extern "C" fn prism_log(
    level: PrismLogLevel,
    source: *const c_char,
    message: *const c_char,
) {
    let handler_data = {
        let store = MOCK_LOG_HANDLER.lock().unwrap();
        *store
    };
    if let Some((cb, ud)) = handler_data {
        cb(ud as *mut c_void, level, source, message);
    }
}

#[no_mangle]
unsafe extern "C" fn prism_log_flush() {}

#[no_mangle]
unsafe extern "C" fn prism_log_shutdown() {}

#[no_mangle]
unsafe extern "C" fn prism_version() -> u32 {
    0x00120200
}

#[no_mangle]
unsafe extern "C" fn prism_version_string() -> *const c_char {
    c"0.18.2".as_ptr()
}
