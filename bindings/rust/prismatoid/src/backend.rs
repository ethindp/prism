// SPDX-License-Identifier: MPL-2.0

use crate::audio::{AudioBuffer, AudioFormat};
use crate::error::{check, Error};
use crate::features::BackendFeatures;
use crate::voice::VoiceInfo;
use std::ffi::{c_void, CStr, CString};
use std::marker::PhantomData;

/// An active instance of a speech synthesis or screen reader backend.
///
/// `Backend` manages the lifetime of the native Prism backend and automatically
/// releases it when dropped. In accordance with Prism's concurrency specifications,
/// a single backend instance is `Send` but not `Sync`: concurrent operations on
/// the same backend instance must be externally synchronized.
pub struct Backend {
    raw: *mut prismatoid_sys::PrismBackend,
    _marker: PhantomData<*mut ()>,
}

unsafe impl Send for Backend {}

impl Drop for Backend {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe {
                prismatoid_sys::prism_backend_free(self.raw);
            }
            self.raw = std::ptr::null_mut();
        }
    }
}

impl Backend {
    /// Wraps and initializes a raw PrismBackend pointer.
    ///
    /// # Safety
    /// `raw` must be a valid, non-null pointer returned by a Prism registry function.
    pub unsafe fn from_raw(raw: *mut prismatoid_sys::PrismBackend) -> Result<Self, Error> {
        assert!(!raw.is_null(), "Backend pointer must not be null");
        let res = prismatoid_sys::prism_backend_initialize(raw);
        if res != prismatoid_sys::PrismError::Ok
            && res != prismatoid_sys::PrismError::AlreadyInitialized
        {
            prismatoid_sys::prism_backend_free(raw);
            return Err(Error::from(res));
        }
        Ok(Self {
            raw,
            _marker: PhantomData,
        })
    }

    /// Explicitly initializes the backend. Returns `Ok(())` if initialization succeeds
    /// or if the backend was already initialized.
    pub fn initialize(&mut self) -> Result<(), Error> {
        let res = unsafe { prismatoid_sys::prism_backend_initialize(self.raw) };
        if res == prismatoid_sys::PrismError::Ok
            || res == prismatoid_sys::PrismError::AlreadyInitialized
        {
            Ok(())
        } else {
            Err(Error::from(res))
        }
    }

    /// Returns the underlying raw pointer to the Prism backend.
    pub fn as_raw(&self) -> *mut prismatoid_sys::PrismBackend {
        self.raw
    }

    /// Returns the name of the backend.
    pub fn name(&self) -> Result<String, Error> {
        let ptr = unsafe { prismatoid_sys::prism_backend_name(self.raw) };
        if ptr.is_null() {
            Ok(String::new())
        } else {
            Ok(unsafe { CStr::from_ptr(ptr) }.to_str()?.to_owned())
        }
    }

    /// Returns the feature bitflags supported by this backend.
    pub fn features(&self) -> Result<BackendFeatures, Error> {
        let raw_features = unsafe { prismatoid_sys::prism_backend_get_features(self.raw) };
        Ok(BackendFeatures::from_bits_truncate(raw_features))
    }

    /// Returns whether this backend supports the specified feature.
    pub fn supports(&self, feature: BackendFeatures) -> bool {
        self.features()
            .map(|f| f.contains(feature))
            .unwrap_or(false)
    }

    /// Speaks the given text.
    ///
    /// If `interrupt` is true, ongoing speech is cancelled before this text is spoken.
    pub fn speak(&mut self, text: &str, interrupt: bool) -> Result<(), Error> {
        let c_text = CString::new(text)?;
        check(unsafe { prismatoid_sys::prism_backend_speak(self.raw, c_text.as_ptr(), interrupt) })
    }

    /// Outputs text as braille on supported screen readers.
    pub fn braille(&mut self, text: &str) -> Result<(), Error> {
        let c_text = CString::new(text)?;
        check(unsafe { prismatoid_sys::prism_backend_braille(self.raw, c_text.as_ptr()) })
    }

    /// Outputs text simultaneously to speech and braille.
    pub fn output(&mut self, text: &str, interrupt: bool) -> Result<(), Error> {
        let c_text = CString::new(text)?;
        check(unsafe { prismatoid_sys::prism_backend_output(self.raw, c_text.as_ptr(), interrupt) })
    }

    /// Stops any ongoing speech synthesis immediately.
    pub fn stop(&mut self) -> Result<(), Error> {
        check(unsafe { prismatoid_sys::prism_backend_stop(self.raw) })
    }

    /// Pauses ongoing speech synthesis.
    pub fn pause(&mut self) -> Result<(), Error> {
        check(unsafe { prismatoid_sys::prism_backend_pause(self.raw) })
    }

    /// Resumes previously paused speech synthesis.
    pub fn resume(&mut self) -> Result<(), Error> {
        check(unsafe { prismatoid_sys::prism_backend_resume(self.raw) })
    }

    /// Returns true if the backend is actively speaking.
    pub fn is_speaking(&self) -> Result<bool, Error> {
        let mut speaking = false;
        check(unsafe { prismatoid_sys::prism_backend_is_speaking(self.raw, &mut speaking) })?;
        Ok(speaking)
    }

    /// Sets the playback volume (typically 0.0 to 1.0).
    pub fn set_volume(&mut self, volume: f32) -> Result<(), Error> {
        check(unsafe { prismatoid_sys::prism_backend_set_volume(self.raw, volume) })
    }

    /// Retrieves the current playback volume.
    pub fn volume(&self) -> Result<f32, Error> {
        let mut vol = 0.0;
        check(unsafe { prismatoid_sys::prism_backend_get_volume(self.raw, &mut vol) })?;
        Ok(vol)
    }

    /// Sets the speaking rate multiplier (typically 0.0 to 1.0 or backend-dependent range).
    pub fn set_rate(&mut self, rate: f32) -> Result<(), Error> {
        check(unsafe { prismatoid_sys::prism_backend_set_rate(self.raw, rate) })
    }

    /// Retrieves the current speaking rate multiplier.
    pub fn rate(&self) -> Result<f32, Error> {
        let mut r = 0.0;
        check(unsafe { prismatoid_sys::prism_backend_get_rate(self.raw, &mut r) })?;
        Ok(r)
    }

    /// Sets the voice pitch.
    pub fn set_pitch(&mut self, pitch: f32) -> Result<(), Error> {
        check(unsafe { prismatoid_sys::prism_backend_set_pitch(self.raw, pitch) })
    }

    /// Retrieves the current voice pitch.
    pub fn pitch(&self) -> Result<f32, Error> {
        let mut p = 0.0;
        check(unsafe { prismatoid_sys::prism_backend_get_pitch(self.raw, &mut p) })?;
        Ok(p)
    }

    /// Refreshes the list of installed voices from the system.
    pub fn refresh_voices(&mut self) -> Result<(), Error> {
        check(unsafe { prismatoid_sys::prism_backend_refresh_voices(self.raw) })
    }

    /// Returns the number of available voices on this backend.
    pub fn voices_count(&self) -> Result<usize, Error> {
        let mut count = 0;
        check(unsafe { prismatoid_sys::prism_backend_count_voices(self.raw, &mut count) })?;
        Ok(count)
    }

    /// Retrieves the name of a voice by its zero-based index.
    pub fn voice_name(&self, voice_id: usize) -> Result<String, Error> {
        let mut ptr = std::ptr::null();
        check(unsafe {
            prismatoid_sys::prism_backend_get_voice_name(self.raw, voice_id, &mut ptr)
        })?;
        if ptr.is_null() {
            Ok(String::new())
        } else {
            Ok(unsafe { CStr::from_ptr(ptr) }.to_str()?.to_owned())
        }
    }

    /// Retrieves the language of a voice by its zero-based index.
    pub fn voice_language(&self, voice_id: usize) -> Result<String, Error> {
        let mut ptr = std::ptr::null();
        check(unsafe {
            prismatoid_sys::prism_backend_get_voice_language(self.raw, voice_id, &mut ptr)
        })?;
        if ptr.is_null() {
            Ok(String::new())
        } else {
            Ok(unsafe { CStr::from_ptr(ptr) }.to_str()?.to_owned())
        }
    }

    /// Selects a voice by its zero-based index.
    pub fn set_voice(&mut self, voice_id: usize) -> Result<(), Error> {
        check(unsafe { prismatoid_sys::prism_backend_set_voice(self.raw, voice_id) })
    }

    /// Queries the index of the currently active voice.
    pub fn voice(&self) -> Result<usize, Error> {
        let mut id = 0;
        check(unsafe { prismatoid_sys::prism_backend_get_voice(self.raw, &mut id) })?;
        Ok(id)
    }

    /// Returns a list of all available voices on this backend.
    pub fn voices(&self) -> Result<Vec<VoiceInfo>, Error> {
        let count = self.voices_count()?;
        let mut list = Vec::with_capacity(count);
        for id in 0..count {
            let name = self.voice_name(id)?;
            let language = self.voice_language(id)?;
            list.push(VoiceInfo { id, name, language });
        }
        Ok(list)
    }

    /// Finds the first voice whose name contains `pattern` (case-insensitive).
    pub fn find_voice(&self, pattern: &str) -> Result<Option<VoiceInfo>, Error> {
        let needle = pattern.to_lowercase();
        let all = self.voices()?;
        Ok(all
            .into_iter()
            .find(|v| v.name.to_lowercase().contains(&needle)))
    }

    /// Finds the first voice matching the given language prefix (e.g. "en", "es", "zh").
    pub fn find_voice_by_language(&self, lang_prefix: &str) -> Result<Option<VoiceInfo>, Error> {
        let needle = lang_prefix.to_lowercase();
        let all = self.voices()?;
        Ok(all
            .into_iter()
            .find(|v| v.language.to_lowercase().starts_with(&needle)))
    }

    /// Retrieves the channel count for synthesized streams (e.g. 1 for mono, 2 for stereo).
    pub fn channels(&self) -> Result<usize, Error> {
        let mut count = 0;
        check(unsafe { prismatoid_sys::prism_backend_get_channels(self.raw, &mut count) })?;
        Ok(count)
    }

    /// Retrieves the sample rate in Hertz (e.g. 44100).
    pub fn sample_rate(&self) -> Result<usize, Error> {
        let mut rate = 0;
        check(unsafe { prismatoid_sys::prism_backend_get_sample_rate(self.raw, &mut rate) })?;
        Ok(rate)
    }

    /// Retrieves the bit depth (e.g. 16 or 32).
    pub fn bit_depth(&self) -> Result<usize, Error> {
        let mut depth = 0;
        check(unsafe { prismatoid_sys::prism_backend_get_bit_depth(self.raw, &mut depth) })?;
        Ok(depth)
    }

    /// Retrieves the overall audio format parameters for synthesized audio.
    pub fn audio_format(&self) -> Result<AudioFormat, Error> {
        Ok(AudioFormat {
            channels: self.channels()?,
            sample_rate: self.sample_rate()?,
            bit_depth: self.bit_depth()?,
        })
    }

    /// Synthesizes text in-memory and invokes the callback with chunks of 32-bit float PCM audio.
    ///
    /// The callback receives `(samples: &[f32], channels: usize, sample_rate: usize)`.
    pub fn speak_to_memory<F>(&mut self, text: &str, mut callback: F) -> Result<(), Error>
    where
        F: FnMut(&[f32], usize, usize),
    {
        let c_text = CString::new(text)?;

        unsafe extern "C" fn trampoline<T: FnMut(&[f32], usize, usize)>(
            userdata: *mut c_void,
            samples: *const f32,
            sample_count: usize,
            channels: usize,
            sample_rate: usize,
        ) {
            if userdata.is_null() || samples.is_null() || sample_count == 0 {
                return;
            }
            let closure = &mut *(userdata as *mut T);
            let slice = std::slice::from_raw_parts(samples, sample_count);
            closure(slice, channels, sample_rate);
        }

        let cb_ptr = &mut callback as *mut F as *mut c_void;
        check(unsafe {
            prismatoid_sys::prism_backend_speak_to_memory(
                self.raw,
                c_text.as_ptr(),
                Some(trampoline::<F>),
                cb_ptr,
            )
        })
    }

    /// Convenience method that synthesizes text in-memory and collects all float PCM samples
    /// into a complete [`AudioBuffer`].
    pub fn synthesize(&mut self, text: &str) -> Result<AudioBuffer, Error> {
        let mut accumulated_samples = Vec::new();
        let mut out_channels = 0;
        let mut out_rate = 0;

        self.speak_to_memory(text, |chunk, channels, rate| {
            out_channels = channels;
            out_rate = rate;
            accumulated_samples.extend_from_slice(chunk);
        })?;

        Ok(AudioBuffer::new(
            accumulated_samples,
            out_channels,
            out_rate,
        ))
    }
}
