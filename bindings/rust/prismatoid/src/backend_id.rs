// SPDX-License-Identifier: MPL-2.0

use std::fmt;

/// Unique 64-bit identifier for a speech or screen reader backend in Prism.
#[derive(Copy, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct BackendId(pub u64);

impl BackendId {
    pub const INVALID: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_INVALID);
    pub const SAPI: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_SAPI);
    pub const AV_SPEECH: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_AV_SPEECH);
    pub const VOICE_OVER: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_VOICE_OVER);
    pub const SPEECH_DISPATCHER: BackendId =
        BackendId(prismatoid_sys::PRISM_BACKEND_SPEECH_DISPATCHER);
    pub const NVDA: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_NVDA);
    pub const JAWS: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_JAWS);
    pub const ONE_CORE: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_ONE_CORE);
    pub const ORCA: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_ORCA);
    pub const ANDROID_SCREEN_READER: BackendId =
        BackendId(prismatoid_sys::PRISM_BACKEND_ANDROID_SCREEN_READER);
    pub const ANDROID_TTS: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_ANDROID_TTS);
    pub const WEB_SPEECH: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_WEB_SPEECH);
    pub const UIA: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_UIA);
    pub const ZDSR: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_ZDSR);
    pub const ZOOM_TEXT: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_ZOOM_TEXT);
    pub const BOY_PC_READER: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_BOY_PC_READER);
    pub const PC_TALKER: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_PC_TALKER);
    pub const SENSE_READER: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_SENSE_READER);
    pub const SYSTEM_ACCESS: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_SYSTEM_ACCESS);
    pub const WINDOW_EYES: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_WINDOW_EYES);
    pub const SPIEL: BackendId = BackendId(prismatoid_sys::PRISM_BACKEND_SPIEL);

    /// Constructs a BackendId from its raw 64-bit integer representation.
    pub const fn from_raw(raw: u64) -> Self {
        Self(raw)
    }

    /// Returns the underlying raw 64-bit identifier.
    pub const fn raw(self) -> u64 {
        self.0
    }

    /// Returns true if this identifier represents a valid, recognized backend ID.
    pub fn is_valid(self) -> bool {
        self.0 != prismatoid_sys::PRISM_BACKEND_INVALID
    }

    /// Returns the canonical static name string if this is a standard known backend.
    pub fn standard_name(self) -> Option<&'static str> {
        match self {
            Self::SAPI => Some("SAPI"),
            Self::AV_SPEECH => Some("AVSpeech"),
            Self::VOICE_OVER => Some("VoiceOver"),
            Self::SPEECH_DISPATCHER => Some("Speech Dispatcher"),
            Self::NVDA => Some("NVDA"),
            Self::JAWS => Some("JAWS"),
            Self::ONE_CORE => Some("OneCore"),
            Self::ORCA => Some("Orca"),
            Self::ANDROID_SCREEN_READER => Some("Android Screen Reader"),
            Self::ANDROID_TTS => Some("Android TTS"),
            Self::WEB_SPEECH => Some("Web Speech"),
            Self::UIA => Some("UI Automation"),
            Self::ZDSR => Some("ZDSR"),
            Self::ZOOM_TEXT => Some("ZoomText"),
            Self::BOY_PC_READER => Some("Boy PC Reader"),
            Self::PC_TALKER => Some("PC-Talker"),
            Self::SENSE_READER => Some("Sense Reader"),
            Self::SYSTEM_ACCESS => Some("System Access"),
            Self::WINDOW_EYES => Some("Window-Eyes"),
            Self::SPIEL => Some("Spiel"),
            _ => None,
        }
    }
}

impl fmt::Debug for BackendId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if let Some(name) = self.standard_name() {
            write!(f, "BackendId({} [0x{:016X}])", name, self.0)
        } else {
            write!(f, "BackendId(0x{:016X})", self.0)
        }
    }
}

impl fmt::Display for BackendId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        if let Some(name) = self.standard_name() {
            write!(f, "{}", name)
        } else {
            write!(f, "0x{:016X}", self.0)
        }
    }
}
