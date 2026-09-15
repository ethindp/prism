// SPDX-License-Identifier: MPL-2.0

use bitflags::bitflags;

bitflags! {
    /// Capabilities and features advertised by a Prism backend.
    #[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
    pub struct BackendFeatures: u64 {
        /// The backend is supported and available on this platform at runtime.
        const IS_SUPPORTED_AT_RUNTIME = prismatoid_sys::PRISM_BACKEND_IS_SUPPORTED_AT_RUNTIME;

        /// Supports standard speech output via `speak()`.
        const SUPPORTS_SPEAK = prismatoid_sys::PRISM_BACKEND_SUPPORTS_SPEAK;

        /// Supports in-memory speech synthesis to float PCM buffers via `speak_to_memory()`.
        const SUPPORTS_SPEAK_TO_MEMORY = prismatoid_sys::PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY;

        /// Supports braille display output via `braille()`.
        const SUPPORTS_BRAILLE = prismatoid_sys::PRISM_BACKEND_SUPPORTS_BRAILLE;

        /// Supports combined speech and braille output via `output()`.
        const SUPPORTS_OUTPUT = prismatoid_sys::PRISM_BACKEND_SUPPORTS_OUTPUT;

        /// Supports querying if speech is currently active via `is_speaking()`.
        const SUPPORTS_IS_SPEAKING = prismatoid_sys::PRISM_BACKEND_SUPPORTS_IS_SPEAKING;

        /// Supports stopping active speech via `stop()`.
        const SUPPORTS_STOP = prismatoid_sys::PRISM_BACKEND_SUPPORTS_STOP;

        /// Supports pausing speech via `pause()`.
        const SUPPORTS_PAUSE = prismatoid_sys::PRISM_BACKEND_SUPPORTS_PAUSE;

        /// Supports resuming paused speech via `resume()`.
        const SUPPORTS_RESUME = prismatoid_sys::PRISM_BACKEND_SUPPORTS_RESUME;

        /// Supports setting output volume via `set_volume()`.
        const SUPPORTS_SET_VOLUME = prismatoid_sys::PRISM_BACKEND_SUPPORTS_SET_VOLUME;

        /// Supports getting current volume via `get_volume()`.
        const SUPPORTS_GET_VOLUME = prismatoid_sys::PRISM_BACKEND_SUPPORTS_GET_VOLUME;

        /// Supports setting speech rate via `set_rate()`.
        const SUPPORTS_SET_RATE = prismatoid_sys::PRISM_BACKEND_SUPPORTS_SET_RATE;

        /// Supports getting speech rate via `get_rate()`.
        const SUPPORTS_GET_RATE = prismatoid_sys::PRISM_BACKEND_SUPPORTS_GET_RATE;

        /// Supports setting voice pitch via `set_pitch()`.
        const SUPPORTS_SET_PITCH = prismatoid_sys::PRISM_BACKEND_SUPPORTS_SET_PITCH;

        /// Supports getting voice pitch via `get_pitch()`.
        const SUPPORTS_GET_PITCH = prismatoid_sys::PRISM_BACKEND_SUPPORTS_GET_PITCH;

        /// Supports querying or updating installed voices via `refresh_voices()`.
        const SUPPORTS_REFRESH_VOICES = prismatoid_sys::PRISM_BACKEND_SUPPORTS_REFRESH_VOICES;

        /// Supports counting installed voices via `count_voices()`.
        const SUPPORTS_COUNT_VOICES = prismatoid_sys::PRISM_BACKEND_SUPPORTS_COUNT_VOICES;

        /// Supports querying voice name via `get_voice_name()`.
        const SUPPORTS_GET_VOICE_NAME = prismatoid_sys::PRISM_BACKEND_SUPPORTS_GET_VOICE_NAME;

        /// Supports querying voice language via `get_voice_language()`.
        const SUPPORTS_GET_VOICE_LANGUAGE = prismatoid_sys::PRISM_BACKEND_SUPPORTS_GET_VOICE_LANGUAGE;

        /// Supports querying current voice index via `get_voice()`.
        const SUPPORTS_GET_VOICE = prismatoid_sys::PRISM_BACKEND_SUPPORTS_GET_VOICE;

        /// Supports selecting voice by index via `set_voice()`.
        const SUPPORTS_SET_VOICE = prismatoid_sys::PRISM_BACKEND_SUPPORTS_SET_VOICE;

        /// Supports querying channel count via `get_channels()`.
        const SUPPORTS_GET_CHANNELS = prismatoid_sys::PRISM_BACKEND_SUPPORTS_GET_CHANNELS;

        /// Supports querying sample rate via `get_sample_rate()`.
        const SUPPORTS_GET_SAMPLE_RATE = prismatoid_sys::PRISM_BACKEND_SUPPORTS_GET_SAMPLE_RATE;

        /// Supports querying bit depth via `get_bit_depth()`.
        const SUPPORTS_GET_BIT_DEPTH = prismatoid_sys::PRISM_BACKEND_SUPPORTS_GET_BIT_DEPTH;

        /// Backend performs silence trimming automatically on `speak()`.
        const PERFORMS_SILENCE_TRIMMING_ON_SPEAK = prismatoid_sys::PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK;

        /// Backend performs silence trimming automatically on `speak_to_memory()`.
        const PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY = prismatoid_sys::PRISM_BACKEND_PERFORMS_SILENCE_TRIMMING_ON_SPEAK_TO_MEMORY;

        /// Backend supports SSML tags in `speak()`.
        const SUPPORTS_SPEAK_SSML = prismatoid_sys::PRISM_BACKEND_SUPPORTS_SPEAK_SSML;

        /// Backend supports SSML tags in `speak_to_memory()`.
        const SUPPORTS_SPEAK_TO_MEMORY_SSML = prismatoid_sys::PRISM_BACKEND_SUPPORTS_SPEAK_TO_MEMORY_SSML;
    }
}
