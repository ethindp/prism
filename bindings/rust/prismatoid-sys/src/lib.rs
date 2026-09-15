// SPDX-License-Identifier: MPL-2.0

//! Raw FFI declarations for Prism (`libprism`).
//!
//! Prism is the Platform-agnostic Reader Interface for Speech and Messages.
//! This crate provides direct C ABI bindings corresponding to `prism.h`.

#![allow(non_camel_case_types, non_snake_case)]

use std::ffi::{c_char, c_void};

pub type PrismBackendId = u64;

#[repr(C)]
pub struct PrismContext {
    _private: [u8; 0],
}

#[repr(C)]
pub struct PrismBackend {
    _private: [u8; 0],
}

#[repr(C)]
pub struct PrismRegistry {
    _private: [u8; 0],
}

#[repr(C)]
pub struct PrismRegistryBuilder {
    _private: [u8; 0],
}

pub type PrismAvailabilityCallback = unsafe extern "C" fn(
    userdata: *mut c_void,
    backend: PrismBackendId,
    name: *const c_char,
    available: bool,
);

pub const PRISM_CONFIG_VERSION: u8 = 3;
pub const PRISM_PLUGIN_ABI_VERSION: u64 = 1;

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PrismConfig {
    pub version: u8,
    pub registry: *mut PrismRegistry,
    pub availability_callback: Option<PrismAvailabilityCallback>,
    pub availability_userdata: *mut c_void,
    pub availability_poll_interval_ms: u32,
    pub availability_debounce_samples: u32,
    pub availability_backoff_max_ms: u32,
    pub availability_auto_power_manage: bool,
}

impl Default for PrismConfig {
    fn default() -> Self {
        unsafe { prism_config_init() }
    }
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum PrismError {
    Ok = 0,
    NotInitialized = 1,
    InvalidParam = 2,
    NotImplemented = 3,
    NoVoices = 4,
    VoiceNotFound = 5,
    SpeakFailure = 6,
    MemoryFailure = 7,
    RangeOutOfBounds = 8,
    Internal = 9,
    NotSpeaking = 10,
    NotPaused = 11,
    AlreadyPaused = 12,
    InvalidUtf8 = 13,
    InvalidOperation = 14,
    AlreadyInitialized = 15,
    BackendNotAvailable = 16,
    Unknown = 17,
    InvalidAudioFormat = 18,
    InternalBackendLimitExceeded = 19,
    BackendEnteredUndefinedState = 20,
    LibraryLoadFailed = 21,
    LibraryInvalid = 22,
    IncompatibleAbi = 23,
    Count = 24,
}

pub type PrismAudioCallback = unsafe extern "C" fn(
    userdata: *mut c_void,
    samples: *const f32,
    sample_count: usize,
    channels: usize,
    sample_rate: usize,
);

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PrismBackendVTable {
    pub size: usize,
    pub create: Option<unsafe extern "C" fn(userdata: *mut c_void) -> *mut c_void>,
    pub destroy: Option<unsafe extern "C" fn(instance: *mut c_void)>,
    pub is_supported: Option<unsafe extern "C" fn(instance: *mut c_void) -> bool>,
    pub initialize: Option<unsafe extern "C" fn(instance: *mut c_void) -> PrismError>,
    pub speak: Option<
        unsafe extern "C" fn(
            instance: *mut c_void,
            text: *const c_char,
            interrupt: bool,
        ) -> PrismError,
    >,
    pub speak_to_memory: Option<
        unsafe extern "C" fn(
            instance: *mut c_void,
            text: *const c_char,
            callback: Option<PrismAudioCallback>,
            userdata: *mut c_void,
        ) -> PrismError,
    >,
    pub braille:
        Option<unsafe extern "C" fn(instance: *mut c_void, text: *const c_char) -> PrismError>,
    pub output: Option<
        unsafe extern "C" fn(
            instance: *mut c_void,
            text: *const c_char,
            interrupt: bool,
        ) -> PrismError,
    >,
    pub stop: Option<unsafe extern "C" fn(instance: *mut c_void) -> PrismError>,
    pub pause: Option<unsafe extern "C" fn(instance: *mut c_void) -> PrismError>,
    pub resume: Option<unsafe extern "C" fn(instance: *mut c_void) -> PrismError>,
    pub is_speaking:
        Option<unsafe extern "C" fn(instance: *mut c_void, out_speaking: *mut bool) -> PrismError>,
    pub set_volume: Option<unsafe extern "C" fn(instance: *mut c_void, volume: f32) -> PrismError>,
    pub get_volume:
        Option<unsafe extern "C" fn(instance: *mut c_void, out_volume: *mut f32) -> PrismError>,
    pub set_rate: Option<unsafe extern "C" fn(instance: *mut c_void, rate: f32) -> PrismError>,
    pub get_rate:
        Option<unsafe extern "C" fn(instance: *mut c_void, out_rate: *mut f32) -> PrismError>,
    pub set_pitch: Option<unsafe extern "C" fn(instance: *mut c_void, pitch: f32) -> PrismError>,
    pub get_pitch:
        Option<unsafe extern "C" fn(instance: *mut c_void, out_pitch: *mut f32) -> PrismError>,
    pub refresh_voices: Option<unsafe extern "C" fn(instance: *mut c_void) -> PrismError>,
    pub count_voices:
        Option<unsafe extern "C" fn(instance: *mut c_void, out_count: *mut usize) -> PrismError>,
    pub get_voice_name: Option<
        unsafe extern "C" fn(
            instance: *mut c_void,
            voice_id: usize,
            out_name: *mut *const c_char,
        ) -> PrismError,
    >,
    pub get_voice_language: Option<
        unsafe extern "C" fn(
            instance: *mut c_void,
            voice_id: usize,
            out_language: *mut *const c_char,
        ) -> PrismError,
    >,
    pub set_voice:
        Option<unsafe extern "C" fn(instance: *mut c_void, voice_id: usize) -> PrismError>,
    pub get_voice:
        Option<unsafe extern "C" fn(instance: *mut c_void, out_voice_id: *mut usize) -> PrismError>,
    pub get_channels:
        Option<unsafe extern "C" fn(instance: *mut c_void, out_channels: *mut usize) -> PrismError>,
    pub get_sample_rate: Option<
        unsafe extern "C" fn(instance: *mut c_void, out_sample_rate: *mut usize) -> PrismError,
    >,
    pub get_bit_depth: Option<
        unsafe extern "C" fn(instance: *mut c_void, out_bit_depth: *mut usize) -> PrismError,
    >,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum PrismLogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    None = 5,
}

pub type PrismLogCallback = unsafe extern "C" fn(
    userdata: *mut c_void,
    level: PrismLogLevel,
    source: *const c_char,
    message: *const c_char,
);

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PrismLogHandler {
    pub fn_callback: Option<PrismLogCallback>,
    pub userdata: *mut c_void,
}

// Standard backend identifiers
pub const PRISM_BACKEND_INVALID: PrismBackendId = 0;
pub const PRISM_BACKEND_SAPI: PrismBackendId = 0x1D6DF72422CEEE66;
pub const PRISM_BACKEND_AV_SPEECH: PrismBackendId = 0x28E3429577805C24;
pub const PRISM_BACKEND_VOICE_OVER: PrismBackendId = 0xCB4897961A754BCB;
pub const PRISM_BACKEND_SPEECH_DISPATCHER: PrismBackendId = 0xE3D6F895D949EBFE;
pub const PRISM_BACKEND_NVDA: PrismBackendId = 0x89CC19C5C4AC1A56;
pub const PRISM_BACKEND_JAWS: PrismBackendId = 0xAC3D60E9BD84B53E;
pub const PRISM_BACKEND_ONE_CORE: PrismBackendId = 0x6797D32F0D994CB4;
pub const PRISM_BACKEND_ORCA: PrismBackendId = 0x10AA1FC05A17F96C;
pub const PRISM_BACKEND_ANDROID_SCREEN_READER: PrismBackendId = 0xD199C175AEEC494B;
pub const PRISM_BACKEND_ANDROID_TTS: PrismBackendId = 0xBC175831BFE4E5CC;
pub const PRISM_BACKEND_WEB_SPEECH: PrismBackendId = 0x3572538D44D44A8F;
pub const PRISM_BACKEND_UIA: PrismBackendId = 0x6238F019DB678F8E;
pub const PRISM_BACKEND_ZDSR: PrismBackendId = 0x3D93C56C9E7F2A2E;
pub const PRISM_BACKEND_ZOOM_TEXT: PrismBackendId = 0xAE439D62DC7B1479;
pub const PRISM_BACKEND_BOY_PC_READER: PrismBackendId = 0x285aba1c16f3300f;
pub const PRISM_BACKEND_PC_TALKER: PrismBackendId = 0x344B951962E3B835;
pub const PRISM_BACKEND_SENSE_READER: PrismBackendId = 0xED4760890B55C2F2;
pub const PRISM_BACKEND_SYSTEM_ACCESS: PrismBackendId = 0x8380F2A37B2C3EB6;
pub const PRISM_BACKEND_WINDOW_EYES: PrismBackendId = 0x9120D89908785C13;
pub const PRISM_BACKEND_SPIEL: PrismBackendId = 0x478B44F14AD3D89C;

// Backend feature bitflags
pub const PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME: u64 = 1 << 0;
pub const PRISM_BACKEND_SUPPORTS_SPEAK: u64 = 1 << 2;
pub const PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY: u64 = 1 << 3;
pub const PRISM_BACKEND_SUPPORTS_BRAILLE: u64 = 1 << 4;
pub const PRISM_BACKEND_SUPPORTS_OUTPUT: u64 = 1 << 5;
pub const PRISM_BACKEND_SUPPORTS_IS_SPEAKING: u64 = 1 << 6;
pub const PRISM_BACKEND_SUPPORTS_STOP: u64 = 1 << 7;
pub const PRISM_BACKEND_SUPPORTS_PAUSE: u64 = 1 << 8;
pub const PRISM_BACKEND_SUPPORTS_RESUME: u64 = 1 << 9;
pub const PRISM_BACKEND_SUPPORTS_SET_VOLUME: u64 = 1 << 10;
pub const PRISM_BACKEND_SUPPORTS_GET_VOLUME: u64 = 1 << 11;
pub const PRISM_BACKEND_SUPPORTS_SET_RATE: u64 = 1 << 12;
pub const PRISM_BACKEND_SUPPORTS_GET_RATE: u64 = 1 << 13;
pub const PRISM_BACKEND_SUPPORTS_SET_PITCH: u64 = 1 << 14;
pub const PRISM_BACKEND_SUPPORTS_GET_PITCH: u64 = 1 << 15;
pub const PRISM_BACKEND_SUPPORTS_REFRESH_VOICES: u64 = 1 << 16;
pub const PRISM_BACKEND_SUPPORTS_COUNT_VOICES: u64 = 1 << 17;
pub const PRISM_BACKEND_SUPPORTS_GET_VOICE_NAME: u64 = 1 << 18;
pub const PRISM_BACKEND_SUPPORTS_GET_VOICE_LANGUAGE: u64 = 1 << 19;
pub const PRISM_BACKEND_SUPPORTS_GET_VOICE: u64 = 1 << 20;
pub const PRISM_BACKEND_SUPPORTS_SET_VOICE: u64 = 1 << 21;
pub const PRISM_BACKEND_SUPPORTS_GET_CHANNELS: u64 = 1 << 22;
pub const PRISM_BACKEND_SUPPORTS_GET_SAMPLE_RATE: u64 = 1 << 23;
pub const PRISM_BACKEND_SUPPORTS_GET_BIT_DEPTH: u64 = 1 << 24;
pub const PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK: u64 = 1 << 25;
pub const PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY: u64 = 1 << 26;
pub const PRISM_BACKEND_SUPPORTS_SPEAK_SSML: u64 = 1 << 27;
pub const PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY_SSML: u64 = 1 << 28;

extern "C" {
    pub fn prism_config_init() -> PrismConfig;
    pub fn prism_init(cfg: *mut PrismConfig) -> *mut PrismContext;
    pub fn prism_shutdown(ctx: *mut PrismContext);
    pub fn prism_availability_poll_pause(ctx: *mut PrismContext);
    pub fn prism_availability_poll_resume(ctx: *mut PrismContext);
    pub fn prism_availability_auto_power_supported() -> bool;

    pub fn prism_registry_count(ctx: *mut PrismContext) -> usize;
    pub fn prism_registry_id_at(ctx: *mut PrismContext, index: usize) -> PrismBackendId;
    pub fn prism_registry_id(ctx: *mut PrismContext, name: *const c_char) -> PrismBackendId;
    pub fn prism_registry_name(ctx: *mut PrismContext, id: PrismBackendId) -> *const c_char;
    pub fn prism_registry_priority(ctx: *mut PrismContext, id: PrismBackendId) -> i32;
    pub fn prism_registry_exists(ctx: *mut PrismContext, id: PrismBackendId) -> bool;
    pub fn prism_registry_get(ctx: *mut PrismContext, id: PrismBackendId) -> *mut PrismBackend;
    pub fn prism_registry_create(ctx: *mut PrismContext, id: PrismBackendId) -> *mut PrismBackend;
    pub fn prism_registry_create_best(ctx: *mut PrismContext) -> *mut PrismBackend;
    pub fn prism_registry_acquire(ctx: *mut PrismContext, id: PrismBackendId) -> *mut PrismBackend;
    pub fn prism_registry_acquire_best(ctx: *mut PrismContext) -> *mut PrismBackend;

    pub fn prism_backend_free(backend: *mut PrismBackend);
    pub fn prism_backend_name(backend: *mut PrismBackend) -> *const c_char;
    pub fn prism_backend_get_features(backend: *mut PrismBackend) -> u64;
    pub fn prism_backend_initialize(backend: *mut PrismBackend) -> PrismError;
    pub fn prism_backend_speak(
        backend: *mut PrismBackend,
        text: *const c_char,
        interrupt: bool,
    ) -> PrismError;
    pub fn prism_backend_speak_to_memory(
        backend: *mut PrismBackend,
        text: *const c_char,
        callback: Option<PrismAudioCallback>,
        userdata: *mut c_void,
    ) -> PrismError;
    pub fn prism_backend_braille(backend: *mut PrismBackend, text: *const c_char) -> PrismError;
    pub fn prism_backend_output(
        backend: *mut PrismBackend,
        text: *const c_char,
        interrupt: bool,
    ) -> PrismError;
    pub fn prism_backend_stop(backend: *mut PrismBackend) -> PrismError;
    pub fn prism_backend_pause(backend: *mut PrismBackend) -> PrismError;
    pub fn prism_backend_resume(backend: *mut PrismBackend) -> PrismError;
    pub fn prism_backend_is_speaking(
        backend: *mut PrismBackend,
        out_speaking: *mut bool,
    ) -> PrismError;
    pub fn prism_backend_set_volume(backend: *mut PrismBackend, volume: f32) -> PrismError;
    pub fn prism_backend_get_volume(backend: *mut PrismBackend, out_volume: *mut f32)
        -> PrismError;
    pub fn prism_backend_set_rate(backend: *mut PrismBackend, rate: f32) -> PrismError;
    pub fn prism_backend_get_rate(backend: *mut PrismBackend, out_rate: *mut f32) -> PrismError;
    pub fn prism_backend_set_pitch(backend: *mut PrismBackend, pitch: f32) -> PrismError;
    pub fn prism_backend_get_pitch(backend: *mut PrismBackend, out_pitch: *mut f32) -> PrismError;
    pub fn prism_backend_refresh_voices(backend: *mut PrismBackend) -> PrismError;
    pub fn prism_backend_count_voices(
        backend: *mut PrismBackend,
        out_count: *mut usize,
    ) -> PrismError;
    pub fn prism_backend_get_voice_name(
        backend: *mut PrismBackend,
        voice_id: usize,
        out_name: *mut *const c_char,
    ) -> PrismError;
    pub fn prism_backend_get_voice_language(
        backend: *mut PrismBackend,
        voice_id: usize,
        out_language: *mut *const c_char,
    ) -> PrismError;
    pub fn prism_backend_set_voice(backend: *mut PrismBackend, voice_id: usize) -> PrismError;
    pub fn prism_backend_get_voice(
        backend: *mut PrismBackend,
        out_voice_id: *mut usize,
    ) -> PrismError;
    pub fn prism_backend_get_channels(
        backend: *mut PrismBackend,
        out_channels: *mut usize,
    ) -> PrismError;
    pub fn prism_backend_get_sample_rate(
        backend: *mut PrismBackend,
        out_sample_rate: *mut usize,
    ) -> PrismError;
    pub fn prism_backend_get_bit_depth(
        backend: *mut PrismBackend,
        out_bit_depth: *mut usize,
    ) -> PrismError;
    pub fn prism_error_string(error: PrismError) -> *const c_char;

    pub fn prism_registry_builder_new() -> *mut PrismRegistryBuilder;
    pub fn prism_registry_builder_add_backend(
        builder: *mut PrismRegistryBuilder,
        name: *const c_char,
        priority: i32,
        features: u64,
        vtable: *const PrismBackendVTable,
        userdata: *mut c_void,
        userdata_free: Option<unsafe extern "C" fn(userdata: *mut c_void)>,
        out_id: *mut PrismBackendId,
    ) -> PrismError;
    pub fn prism_registry_builder_add_library(
        builder: *mut PrismRegistryBuilder,
        path: *const c_char,
        priority_override: i32,
        out_count: *mut usize,
    ) -> PrismError;
    pub fn prism_registry_freeze(builder: *mut PrismRegistryBuilder) -> *mut PrismRegistry;
    pub fn prism_registry_builder_free(builder: *mut PrismRegistryBuilder);
    pub fn prism_registry_retain(registry: *mut PrismRegistry) -> *mut PrismRegistry;
    pub fn prism_registry_release(registry: *mut PrismRegistry);

    pub fn prism_set_log_handler(handler: PrismLogHandler) -> PrismLogHandler;
    pub fn prism_set_log_level(level: PrismLogLevel) -> PrismLogLevel;
    pub fn prism_log(level: PrismLogLevel, source: *const c_char, message: *const c_char);
    pub fn prism_log_flush();
    pub fn prism_log_shutdown();
    pub fn prism_version() -> u32;
    pub fn prism_version_string() -> *const c_char;
}

#[cfg(feature = "mock")]
pub mod mock;
#[cfg(feature = "mock")]
pub use mock::mock_trigger_availability;
