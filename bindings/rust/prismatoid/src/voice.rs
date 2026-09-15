// SPDX-License-Identifier: MPL-2.0

use std::fmt;

/// Information describing an installed TTS voice on a backend.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VoiceInfo {
    /// Zero-based voice identifier index used in `set_voice()`.
    pub id: usize,
    /// Human-readable name of the voice (e.g. "Microsoft David").
    pub name: String,
    /// BCP-47 or standard language tag (e.g. "en-US").
    pub language: String,
}

impl fmt::Display for VoiceInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} ({}) [#{}]", self.name, self.language, self.id)
    }
}
