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
}

// Trampolines for C ABI
unsafe extern "C" fn custom_create<B: CustomBackend, F: Fn() -> B>(
    userdata: *mut c_void,
) -> *mut c_void {
    let factory = &*(userdata as *const F);
    let instance = Box::new(factory());
    Box::into_raw(instance) as *mut c_void
}

unsafe extern "C" fn custom_destroy<B: CustomBackend>(instance: *mut c_void) {
    if !instance.is_null() {
        drop(Box::from_raw(instance as *mut B));
    }
}

unsafe extern "C" fn custom_userdata_free<F>(userdata: *mut c_void) {
    if !userdata.is_null() {
        drop(Box::from_raw(userdata as *mut F));
    }
}

unsafe extern "C" fn custom_is_supported<B: CustomBackend>(instance: *mut c_void) -> bool {
    let b = &*(instance as *mut B);
    b.is_supported()
}

unsafe extern "C" fn custom_initialize<B: CustomBackend>(
    instance: *mut c_void,
) -> prismatoid_sys::PrismError {
    let b = &mut *(instance as *mut B);
    match b.initialize() {
        Ok(()) => prismatoid_sys::PrismError::Ok,
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_speak<B: CustomBackend>(
    instance: *mut c_void,
    text: *const c_char,
    interrupt: bool,
) -> prismatoid_sys::PrismError {
    if text.is_null() {
        return prismatoid_sys::PrismError::InvalidParam;
    }
    let s = match CStr::from_ptr(text).to_str() {
        Ok(s) => s,
        Err(_) => return prismatoid_sys::PrismError::InvalidUtf8,
    };
    let b = &mut *(instance as *mut B);
    match b.speak(s, interrupt) {
        Ok(()) => prismatoid_sys::PrismError::Ok,
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_stop<B: CustomBackend>(
    instance: *mut c_void,
) -> prismatoid_sys::PrismError {
    let b = &mut *(instance as *mut B);
    match b.stop() {
        Ok(()) => prismatoid_sys::PrismError::Ok,
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_pause<B: CustomBackend>(
    instance: *mut c_void,
) -> prismatoid_sys::PrismError {
    let b = &mut *(instance as *mut B);
    match b.pause() {
        Ok(()) => prismatoid_sys::PrismError::Ok,
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_resume<B: CustomBackend>(
    instance: *mut c_void,
) -> prismatoid_sys::PrismError {
    let b = &mut *(instance as *mut B);
    match b.resume() {
        Ok(()) => prismatoid_sys::PrismError::Ok,
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_is_speaking<B: CustomBackend>(
    instance: *mut c_void,
    out_speaking: *mut bool,
) -> prismatoid_sys::PrismError {
    if out_speaking.is_null() {
        return prismatoid_sys::PrismError::InvalidParam;
    }
    let b = &*(instance as *mut B);
    match b.is_speaking() {
        Ok(sp) => {
            *out_speaking = sp;
            prismatoid_sys::PrismError::Ok
        }
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_set_volume<B: CustomBackend>(
    instance: *mut c_void,
    volume: f32,
) -> prismatoid_sys::PrismError {
    let b = &mut *(instance as *mut B);
    match b.set_volume(volume) {
        Ok(()) => prismatoid_sys::PrismError::Ok,
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_get_volume<B: CustomBackend>(
    instance: *mut c_void,
    out_volume: *mut f32,
) -> prismatoid_sys::PrismError {
    if out_volume.is_null() {
        return prismatoid_sys::PrismError::InvalidParam;
    }
    let b = &*(instance as *mut B);
    match b.volume() {
        Ok(vol) => {
            *out_volume = vol;
            prismatoid_sys::PrismError::Ok
        }
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_set_rate<B: CustomBackend>(
    instance: *mut c_void,
    rate: f32,
) -> prismatoid_sys::PrismError {
    let b = &mut *(instance as *mut B);
    match b.set_rate(rate) {
        Ok(()) => prismatoid_sys::PrismError::Ok,
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_get_rate<B: CustomBackend>(
    instance: *mut c_void,
    out_rate: *mut f32,
) -> prismatoid_sys::PrismError {
    if out_rate.is_null() {
        return prismatoid_sys::PrismError::InvalidParam;
    }
    let b = &*(instance as *mut B);
    match b.rate() {
        Ok(r) => {
            *out_rate = r;
            prismatoid_sys::PrismError::Ok
        }
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_set_pitch<B: CustomBackend>(
    instance: *mut c_void,
    pitch: f32,
) -> prismatoid_sys::PrismError {
    let b = &mut *(instance as *mut B);
    match b.set_pitch(pitch) {
        Ok(()) => prismatoid_sys::PrismError::Ok,
        Err(e) => e.into(),
    }
}

unsafe extern "C" fn custom_get_pitch<B: CustomBackend>(
    instance: *mut c_void,
    out_pitch: *mut f32,
) -> prismatoid_sys::PrismError {
    if out_pitch.is_null() {
        return prismatoid_sys::PrismError::InvalidParam;
    }
    let b = &*(instance as *mut B);
    match b.pitch() {
        Ok(p) => {
            *out_pitch = p;
            prismatoid_sys::PrismError::Ok
        }
        Err(e) => e.into(),
    }
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
            speak_to_memory: None,
            braille: None,
            output: None,
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
            refresh_voices: None,
            count_voices: None,
            get_voice_name: None,
            get_voice_language: None,
            set_voice: None,
            get_voice: None,
            get_channels: None,
            get_sample_rate: None,
            get_bit_depth: None,
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
    pub fn add_library(&mut self, path: &str, priority_override: i32) -> Result<usize, Error> {
        let c_path = CString::new(path)?;
        let mut count = 0;
        check(unsafe {
            prismatoid_sys::prism_registry_builder_add_library(
                self.raw,
                c_path.as_ptr(),
                priority_override,
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
