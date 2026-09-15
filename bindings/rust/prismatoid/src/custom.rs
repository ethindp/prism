// SPDX-License-Identifier: MPL-2.0

use crate::backend_id::BackendId;
use crate::error::{check, Error};
use crate::features::BackendFeatures;
use std::ffi::{c_char, c_void, CStr, CString};

/// Trait for implementing custom speech or screen reader backends in Rust.
pub trait CustomBackend: Send + 'static {
    /// Returns the name of the custom backend.
    fn name(&self) -> &str;

    /// Checks whether this backend is currently supported on the host system.
    fn is_supported(&self) -> bool {
        true
    }

    /// Initializes the backend instance.
    fn initialize(&mut self) -> Result<(), Error> {
        Ok(())
    }

    /// Speaks the given text.
    fn speak(&mut self, _text: &str, _interrupt: bool) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Outputs text to a braille display.
    fn braille(&mut self, _text: &str) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Stops speech output.
    fn stop(&mut self) -> Result<(), Error> {
        Ok(())
    }

    /// Pauses speech output.
    fn pause(&mut self) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Resumes speech output.
    fn resume(&mut self) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Returns true if the backend is currently speaking.
    fn is_speaking(&self) -> Result<bool, Error> {
        Ok(false)
    }

    /// Sets the playback volume (0.0 to 1.0).
    fn set_volume(&mut self, _volume: f32) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Retrieves the current playback volume.
    fn volume(&self) -> Result<f32, Error> {
        Ok(1.0)
    }

    /// Sets the speaking rate multiplier.
    fn set_rate(&mut self, _rate: f32) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Retrieves the current speaking rate multiplier.
    fn rate(&self) -> Result<f32, Error> {
        Ok(1.0)
    }

    /// Sets the voice pitch.
    fn set_pitch(&mut self, _pitch: f32) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Retrieves the current voice pitch.
    fn pitch(&self) -> Result<f32, Error> {
        Ok(1.0)
    }

    /// Synthesizes speech to memory, emitting audio chunks to the provided closure.
    fn speak_to_memory(
        &mut self,
        _text: &str,
        _emit: &mut dyn FnMut(&[f32], usize, usize),
    ) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Outputs text simultaneously to speech and braille.
    fn output(&mut self, _text: &str, _interrupt: bool) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Refreshes the list of installed voices.
    fn refresh_voices(&mut self) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Returns the number of available voices.
    fn count_voices(&self) -> Result<usize, Error> {
        Err(Error::NotImplemented)
    }

    /// Retrieves the name of a voice by its index.
    fn voice_name(&self, _id: usize) -> Result<String, Error> {
        Err(Error::NotImplemented)
    }

    /// Retrieves the language of a voice by its index.
    fn voice_language(&self, _id: usize) -> Result<String, Error> {
        Err(Error::NotImplemented)
    }

    /// Selects an active voice by its index.
    fn set_voice(&mut self, _id: usize) -> Result<(), Error> {
        Err(Error::NotImplemented)
    }

    /// Retrieves the index of the currently active voice.
    fn voice(&self) -> Result<usize, Error> {
        Err(Error::NotImplemented)
    }

    /// Retrieves the channel count for synthesized streams.
    fn channels(&self) -> Result<usize, Error> {
        Err(Error::NotImplemented)
    }

    /// Retrieves the sample rate in Hertz for synthesized streams.
    fn sample_rate(&self) -> Result<usize, Error> {
        Err(Error::NotImplemented)
    }

    /// Retrieves the bit depth for synthesized streams.
    fn bit_depth(&self) -> Result<usize, Error> {
        Err(Error::NotImplemented)
    }
}

struct CustomBackendInstance<B: CustomBackend> {
    backend: B,
    cached_voice_name: Option<CString>,
    cached_voice_language: Option<CString>,
}

// Trampolines for C ABI
unsafe extern "C" fn custom_create<B: CustomBackend, F: Fn() -> B>(
    userdata: *mut c_void,
) -> *mut c_void {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if userdata.is_null() {
            return std::ptr::null_mut();
        }
        let factory = &*(userdata as *const F);
        let instance = Box::new(CustomBackendInstance {
            backend: factory(),
            cached_voice_name: None,
            cached_voice_language: None,
        });
        Box::into_raw(instance) as *mut c_void
    }));
    result.unwrap_or(std::ptr::null_mut())
}

unsafe extern "C" fn custom_destroy<B: CustomBackend>(instance: *mut c_void) {
    if !instance.is_null() {
        let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            drop(Box::from_raw(instance as *mut CustomBackendInstance<B>));
        }));
    }
}

unsafe extern "C" fn custom_userdata_free<F>(userdata: *mut c_void) {
    if !userdata.is_null() {
        let _ = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
            drop(Box::from_raw(userdata as *mut F));
        }));
    }
}

unsafe extern "C" fn custom_is_supported<B: CustomBackend>(instance: *mut c_void) -> bool {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return false;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        inst.backend.is_supported()
    }));
    result.unwrap_or(false)
}

unsafe extern "C" fn custom_initialize<B: CustomBackend>(
    instance: *mut c_void,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.initialize() {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_speak<B: CustomBackend>(
    instance: *mut c_void,
    text: *const c_char,
    interrupt: bool,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || text.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let s = match CStr::from_ptr(text).to_str() {
            Ok(s) => s,
            Err(_) => return prismatoid_sys::PrismError::InvalidUtf8,
        };
        if s.is_empty() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.speak(s, interrupt) {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_speak_to_memory<B: CustomBackend>(
    instance: *mut c_void,
    text: *const c_char,
    callback: Option<prismatoid_sys::PrismAudioCallback>,
    userdata: *mut c_void,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || text.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let s = match CStr::from_ptr(text).to_str() {
            Ok(s) => s,
            Err(_) => return prismatoid_sys::PrismError::InvalidUtf8,
        };
        if s.is_empty() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        let mut emit = |samples: &[f32], channels: usize, sample_rate: usize| {
            if let Some(cb) = callback {
                cb(
                    userdata,
                    samples.as_ptr(),
                    samples.len(),
                    channels,
                    sample_rate,
                );
            }
        };
        match inst.backend.speak_to_memory(s, &mut emit) {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_braille<B: CustomBackend>(
    instance: *mut c_void,
    text: *const c_char,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || text.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let s = match CStr::from_ptr(text).to_str() {
            Ok(s) => s,
            Err(_) => return prismatoid_sys::PrismError::InvalidUtf8,
        };
        if s.is_empty() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.braille(s) {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_output<B: CustomBackend>(
    instance: *mut c_void,
    text: *const c_char,
    interrupt: bool,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || text.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let s = match CStr::from_ptr(text).to_str() {
            Ok(s) => s,
            Err(_) => return prismatoid_sys::PrismError::InvalidUtf8,
        };
        if s.is_empty() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.output(s, interrupt) {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_stop<B: CustomBackend>(
    instance: *mut c_void,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.stop() {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_pause<B: CustomBackend>(
    instance: *mut c_void,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.pause() {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_resume<B: CustomBackend>(
    instance: *mut c_void,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.resume() {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_is_speaking<B: CustomBackend>(
    instance: *mut c_void,
    out_speaking: *mut bool,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_speaking.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        match inst.backend.is_speaking() {
            Ok(sp) => {
                *out_speaking = sp;
                prismatoid_sys::PrismError::Ok
            }
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_set_volume<B: CustomBackend>(
    instance: *mut c_void,
    volume: f32,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.set_volume(volume) {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_get_volume<B: CustomBackend>(
    instance: *mut c_void,
    out_volume: *mut f32,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_volume.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        match inst.backend.volume() {
            Ok(vol) => {
                *out_volume = vol;
                prismatoid_sys::PrismError::Ok
            }
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_set_rate<B: CustomBackend>(
    instance: *mut c_void,
    rate: f32,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.set_rate(rate) {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_get_rate<B: CustomBackend>(
    instance: *mut c_void,
    out_rate: *mut f32,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_rate.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        match inst.backend.rate() {
            Ok(r) => {
                *out_rate = r;
                prismatoid_sys::PrismError::Ok
            }
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_set_pitch<B: CustomBackend>(
    instance: *mut c_void,
    pitch: f32,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.set_pitch(pitch) {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_get_pitch<B: CustomBackend>(
    instance: *mut c_void,
    out_pitch: *mut f32,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_pitch.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        match inst.backend.pitch() {
            Ok(p) => {
                *out_pitch = p;
                prismatoid_sys::PrismError::Ok
            }
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_refresh_voices<B: CustomBackend>(
    instance: *mut c_void,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.refresh_voices() {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_count_voices<B: CustomBackend>(
    instance: *mut c_void,
    out_count: *mut usize,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_count.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        match inst.backend.count_voices() {
            Ok(count) => {
                *out_count = count;
                prismatoid_sys::PrismError::Ok
            }
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_get_voice_name<B: CustomBackend>(
    instance: *mut c_void,
    voice_id: usize,
    out_name: *mut *const c_char,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_name.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.voice_name(voice_id) {
            Ok(name) => match CString::new(name) {
                Ok(c_name) => {
                    inst.cached_voice_name = Some(c_name);
                    *out_name = inst.cached_voice_name.as_ref().unwrap().as_ptr();
                    prismatoid_sys::PrismError::Ok
                }
                Err(e) => Error::from(e).into(),
            },
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_get_voice_language<B: CustomBackend>(
    instance: *mut c_void,
    voice_id: usize,
    out_language: *mut *const c_char,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_language.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.voice_language(voice_id) {
            Ok(lang) => match CString::new(lang) {
                Ok(c_lang) => {
                    inst.cached_voice_language = Some(c_lang);
                    *out_language = inst.cached_voice_language.as_ref().unwrap().as_ptr();
                    prismatoid_sys::PrismError::Ok
                }
                Err(e) => Error::from(e).into(),
            },
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_set_voice<B: CustomBackend>(
    instance: *mut c_void,
    voice_id: usize,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &mut *(instance as *mut CustomBackendInstance<B>);
        match inst.backend.set_voice(voice_id) {
            Ok(()) => prismatoid_sys::PrismError::Ok,
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_get_voice<B: CustomBackend>(
    instance: *mut c_void,
    out_voice_id: *mut usize,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_voice_id.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        match inst.backend.voice() {
            Ok(id) => {
                *out_voice_id = id;
                prismatoid_sys::PrismError::Ok
            }
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_get_channels<B: CustomBackend>(
    instance: *mut c_void,
    out_channels: *mut usize,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_channels.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        match inst.backend.channels() {
            Ok(c) => {
                *out_channels = c;
                prismatoid_sys::PrismError::Ok
            }
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_get_sample_rate<B: CustomBackend>(
    instance: *mut c_void,
    out_sample_rate: *mut usize,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_sample_rate.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        match inst.backend.sample_rate() {
            Ok(sr) => {
                *out_sample_rate = sr;
                prismatoid_sys::PrismError::Ok
            }
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

unsafe extern "C" fn custom_get_bit_depth<B: CustomBackend>(
    instance: *mut c_void,
    out_bit_depth: *mut usize,
) -> prismatoid_sys::PrismError {
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        if instance.is_null() || out_bit_depth.is_null() {
            return prismatoid_sys::PrismError::InvalidParam;
        }
        let inst = &*(instance as *const CustomBackendInstance<B>);
        match inst.backend.bit_depth() {
            Ok(bd) => {
                *out_bit_depth = bd;
                prismatoid_sys::PrismError::Ok
            }
            Err(e) => e.into(),
        }
    }));
    result.unwrap_or(prismatoid_sys::PrismError::Internal)
}

/// A custom frozen backend registry.
pub struct Registry {
    raw: *mut prismatoid_sys::PrismRegistry,
}

unsafe impl Send for Registry {}
unsafe impl Sync for Registry {}

impl Drop for Registry {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                prismatoid_sys::prism_registry_release(self.raw);
            }
            self.raw = std::ptr::null_mut();
        }
    }
}

impl Registry {
    /// Returns the underlying raw pointer to the Prism registry.
    pub fn as_raw(&self) -> *mut prismatoid_sys::PrismRegistry {
        self.raw
    }

    /// Increments the reference count of the registry and returns a clone.
    pub fn retain(&self) -> Self {
        unsafe {
            let ptr = prismatoid_sys::prism_registry_retain(self.raw);
            Self { raw: ptr }
        }
    }
}

/// Builder for creating custom registries with user-defined or shared library backends.
pub struct RegistryBuilder {
    raw: *mut prismatoid_sys::PrismRegistryBuilder,
}

impl Drop for RegistryBuilder {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                prismatoid_sys::prism_registry_builder_free(self.raw);
            }
            self.raw = std::ptr::null_mut();
        }
    }
}

impl RegistryBuilder {
    /// Constructs a new `RegistryBuilder`.
    pub fn new() -> Result<Self, Error> {
        let raw = unsafe { prismatoid_sys::prism_registry_builder_new() };
        if raw.is_null() {
            Err(Error::MemoryFailure)
        } else {
            Ok(Self { raw })
        }
    }

    /// Registers a custom backend implementation using a factory closure.
    pub fn add_backend_with_factory<B, F>(
        &mut self,
        name: &str,
        priority: i32,
        features: BackendFeatures,
        factory: F,
    ) -> Result<BackendId, Error>
    where
        B: CustomBackend,
        F: Fn() -> B + Send + Sync + 'static,
    {
        let c_name = CString::new(name)?;
        let userdata = Box::into_raw(Box::new(factory)) as *mut c_void;

        let vtable = prismatoid_sys::PrismBackendVTable {
            size: std::mem::size_of::<prismatoid_sys::PrismBackendVTable>(),
            create: Some(custom_create::<B, F>),
            destroy: Some(custom_destroy::<B>),
            is_supported: Some(custom_is_supported::<B>),
            initialize: Some(custom_initialize::<B>),
            speak: if features.contains(BackendFeatures::SUPPORTS_SPEAK) {
                Some(custom_speak::<B>)
            } else {
                None
            },
            speak_to_memory: if features.contains(BackendFeatures::SUPPORTS_SPEAK_TO_MEMORY) {
                Some(custom_speak_to_memory::<B>)
            } else {
                None
            },
            braille: if features.contains(BackendFeatures::SUPPORTS_BRAILLE) {
                Some(custom_braille::<B>)
            } else {
                None
            },
            output: if features.contains(BackendFeatures::SUPPORTS_OUTPUT) {
                Some(custom_output::<B>)
            } else {
                None
            },
            stop: if features.contains(BackendFeatures::SUPPORTS_STOP) {
                Some(custom_stop::<B>)
            } else {
                None
            },
            pause: if features.contains(BackendFeatures::SUPPORTS_PAUSE) {
                Some(custom_pause::<B>)
            } else {
                None
            },
            resume: if features.contains(BackendFeatures::SUPPORTS_RESUME) {
                Some(custom_resume::<B>)
            } else {
                None
            },
            is_speaking: if features.contains(BackendFeatures::SUPPORTS_IS_SPEAKING) {
                Some(custom_is_speaking::<B>)
            } else {
                None
            },
            set_volume: if features.contains(BackendFeatures::SUPPORTS_SET_VOLUME) {
                Some(custom_set_volume::<B>)
            } else {
                None
            },
            get_volume: if features.contains(BackendFeatures::SUPPORTS_GET_VOLUME) {
                Some(custom_get_volume::<B>)
            } else {
                None
            },
            set_rate: if features.contains(BackendFeatures::SUPPORTS_SET_RATE) {
                Some(custom_set_rate::<B>)
            } else {
                None
            },
            get_rate: if features.contains(BackendFeatures::SUPPORTS_GET_RATE) {
                Some(custom_get_rate::<B>)
            } else {
                None
            },
            set_pitch: if features.contains(BackendFeatures::SUPPORTS_SET_PITCH) {
                Some(custom_set_pitch::<B>)
            } else {
                None
            },
            get_pitch: if features.contains(BackendFeatures::SUPPORTS_GET_PITCH) {
                Some(custom_get_pitch::<B>)
            } else {
                None
            },
            refresh_voices: if features.contains(BackendFeatures::SUPPORTS_REFRESH_VOICES) {
                Some(custom_refresh_voices::<B>)
            } else {
                None
            },
            count_voices: if features.contains(BackendFeatures::SUPPORTS_COUNT_VOICES) {
                Some(custom_count_voices::<B>)
            } else {
                None
            },
            get_voice_name: if features.contains(BackendFeatures::SUPPORTS_GET_VOICE_NAME) {
                Some(custom_get_voice_name::<B>)
            } else {
                None
            },
            get_voice_language: if features.contains(BackendFeatures::SUPPORTS_GET_VOICE_LANGUAGE) {
                Some(custom_get_voice_language::<B>)
            } else {
                None
            },
            set_voice: if features.contains(BackendFeatures::SUPPORTS_SET_VOICE) {
                Some(custom_set_voice::<B>)
            } else {
                None
            },
            get_voice: if features.contains(BackendFeatures::SUPPORTS_GET_VOICE) {
                Some(custom_get_voice::<B>)
            } else {
                None
            },
            get_channels: if features.contains(BackendFeatures::SUPPORTS_GET_CHANNELS) {
                Some(custom_get_channels::<B>)
            } else {
                None
            },
            get_sample_rate: if features.contains(BackendFeatures::SUPPORTS_GET_SAMPLE_RATE) {
                Some(custom_get_sample_rate::<B>)
            } else {
                None
            },
            get_bit_depth: if features.contains(BackendFeatures::SUPPORTS_GET_BIT_DEPTH) {
                Some(custom_get_bit_depth::<B>)
            } else {
                None
            },
        };

        let mut out_id = 0u64;
        let err = unsafe {
            prismatoid_sys::prism_registry_builder_add_backend(
                self.raw,
                c_name.as_ptr(),
                priority,
                features.bits(),
                &vtable,
                userdata,
                Some(custom_userdata_free::<F>),
                &mut out_id,
            )
        };

        check(err)?;
        Ok(BackendId(out_id))
    }

    /// Registers a custom backend implementation using its `Default` constructor.
    pub fn add_backend<B: CustomBackend + Default>(
        &mut self,
        name: &str,
        priority: i32,
        features: BackendFeatures,
    ) -> Result<BackendId, Error> {
        self.add_backend_with_factory(name, priority, features, B::default)
    }

    /// Loads external backend plugins from a shared library path.
    pub fn add_library(
        &mut self,
        path: impl AsRef<std::path::Path>,
        priority_override: Option<i32>,
    ) -> Result<usize, Error> {
        let path_str = path.as_ref().to_str().ok_or(Error::InvalidUtf8)?;
        let c_path = CString::new(path_str)?;
        let prio = priority_override.unwrap_or(-1);
        let mut count = 0;
        check(unsafe {
            prismatoid_sys::prism_registry_builder_add_library(
                self.raw,
                c_path.as_ptr(),
                prio,
                &mut count,
            )
        })?;
        Ok(count)
    }

    /// Freezes the builder and produces a finalized immutable `Registry`.
    pub fn freeze(mut self) -> Result<Registry, Error> {
        let reg = unsafe { prismatoid_sys::prism_registry_freeze(self.raw) };
        self.raw = std::ptr::null_mut();
        if reg.is_null() {
            Err(Error::MemoryFailure)
        } else {
            Ok(Registry { raw: reg })
        }
    }
}
