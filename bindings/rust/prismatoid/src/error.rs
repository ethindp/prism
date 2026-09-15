// SPDX-License-Identifier: MPL-2.0

use std::ffi::CStr;
use thiserror::Error;

/// Errors returned by Prism operations.
#[derive(Debug, Error, Clone, PartialEq, Eq)]
pub enum Error {
    #[error("Prism subsystem or backend is not initialized")]
    NotInitialized,

    #[error("Invalid parameter: {0}")]
    InvalidParam(String),

    #[error("Requested operation is not implemented by this backend")]
    NotImplemented,

    #[error("No voices available on this backend")]
    NoVoices,

    #[error("Voice not found")]
    VoiceNotFound,

    #[error("Speech synthesis failure")]
    SpeakFailure,

    #[error("Memory allocation failure")]
    MemoryFailure,

    #[error("Value out of valid range")]
    RangeOutOfBounds,

    #[error("Internal backend error")]
    Internal,

    #[error("Backend is not currently speaking")]
    NotSpeaking,

    #[error("Backend is not paused")]
    NotPaused,

    #[error("Backend is already paused")]
    AlreadyPaused,

    #[error("Invalid UTF-8 string provided")]
    InvalidUtf8,

    #[error("Invalid operation in current state")]
    InvalidOperation,

    #[error("Backend is already initialized")]
    AlreadyInitialized,

    #[error("Requested backend is not available on this platform")]
    BackendNotAvailable,

    #[error("Invalid audio format")]
    InvalidAudioFormat,

    #[error("Internal backend limit exceeded")]
    InternalBackendLimitExceeded,

    #[error("Backend entered undefined state")]
    BackendEnteredUndefinedState,

    #[error("Failed to load backend library")]
    LibraryLoadFailed,

    #[error("Backend library is invalid or corrupt")]
    LibraryInvalid,

    #[error("Incompatible ABI version")]
    IncompatibleAbi,

    #[error("Unknown or unspecified error")]
    Unknown,

    #[error("String contains an interior NUL byte: {0}")]
    NulError(String),
}

/// Convenience result type alias for Prism operations.
pub type Result<T, E = Error> = std::result::Result<T, E>;

impl From<std::str::Utf8Error> for Error {
    fn from(_: std::str::Utf8Error) -> Self {
        Error::InvalidUtf8
    }
}

impl From<std::ffi::NulError> for Error {
    fn from(err: std::ffi::NulError) -> Self {
        Error::NulError(err.to_string())
    }
}

impl From<prismatoid_sys::PrismError> for Error {
    fn from(err: prismatoid_sys::PrismError) -> Self {
        match err {
            prismatoid_sys::PrismError::Ok => {
                unreachable!("PrismError::Ok cannot be converted to an Error")
            }
            prismatoid_sys::PrismError::NotInitialized => Error::NotInitialized,
            prismatoid_sys::PrismError::InvalidParam => {
                Error::InvalidParam("invalid argument passed to Prism API".into())
            }
            prismatoid_sys::PrismError::NotImplemented => Error::NotImplemented,
            prismatoid_sys::PrismError::NoVoices => Error::NoVoices,
            prismatoid_sys::PrismError::VoiceNotFound => Error::VoiceNotFound,
            prismatoid_sys::PrismError::SpeakFailure => Error::SpeakFailure,
            prismatoid_sys::PrismError::MemoryFailure => Error::MemoryFailure,
            prismatoid_sys::PrismError::RangeOutOfBounds => Error::RangeOutOfBounds,
            prismatoid_sys::PrismError::Internal => Error::Internal,
            prismatoid_sys::PrismError::NotSpeaking => Error::NotSpeaking,
            prismatoid_sys::PrismError::NotPaused => Error::NotPaused,
            prismatoid_sys::PrismError::AlreadyPaused => Error::AlreadyPaused,
            prismatoid_sys::PrismError::InvalidUtf8 => Error::InvalidUtf8,
            prismatoid_sys::PrismError::InvalidOperation => Error::InvalidOperation,
            prismatoid_sys::PrismError::AlreadyInitialized => Error::AlreadyInitialized,
            prismatoid_sys::PrismError::BackendNotAvailable => Error::BackendNotAvailable,
            prismatoid_sys::PrismError::Unknown => Error::Unknown,
            prismatoid_sys::PrismError::InvalidAudioFormat => Error::InvalidAudioFormat,
            prismatoid_sys::PrismError::InternalBackendLimitExceeded => {
                Error::InternalBackendLimitExceeded
            }
            prismatoid_sys::PrismError::BackendEnteredUndefinedState => {
                Error::BackendEnteredUndefinedState
            }
            prismatoid_sys::PrismError::LibraryLoadFailed => Error::LibraryLoadFailed,
            prismatoid_sys::PrismError::LibraryInvalid => Error::LibraryInvalid,
            prismatoid_sys::PrismError::IncompatibleAbi => Error::IncompatibleAbi,
            prismatoid_sys::PrismError::Count => Error::Unknown,
        }
    }
}

impl From<Error> for prismatoid_sys::PrismError {
    fn from(err: Error) -> Self {
        match err {
            Error::NotInitialized => prismatoid_sys::PrismError::NotInitialized,
            Error::InvalidParam(_) | Error::NulError(_) => prismatoid_sys::PrismError::InvalidParam,
            Error::NotImplemented => prismatoid_sys::PrismError::NotImplemented,
            Error::NoVoices => prismatoid_sys::PrismError::NoVoices,
            Error::VoiceNotFound => prismatoid_sys::PrismError::VoiceNotFound,
            Error::SpeakFailure => prismatoid_sys::PrismError::SpeakFailure,
            Error::MemoryFailure => prismatoid_sys::PrismError::MemoryFailure,
            Error::RangeOutOfBounds => prismatoid_sys::PrismError::RangeOutOfBounds,
            Error::Internal => prismatoid_sys::PrismError::Internal,
            Error::NotSpeaking => prismatoid_sys::PrismError::NotSpeaking,
            Error::NotPaused => prismatoid_sys::PrismError::NotPaused,
            Error::AlreadyPaused => prismatoid_sys::PrismError::AlreadyPaused,
            Error::InvalidUtf8 => prismatoid_sys::PrismError::InvalidUtf8,
            Error::InvalidOperation => prismatoid_sys::PrismError::InvalidOperation,
            Error::AlreadyInitialized => prismatoid_sys::PrismError::AlreadyInitialized,
            Error::BackendNotAvailable => prismatoid_sys::PrismError::BackendNotAvailable,
            Error::InvalidAudioFormat => prismatoid_sys::PrismError::InvalidAudioFormat,
            Error::InternalBackendLimitExceeded => {
                prismatoid_sys::PrismError::InternalBackendLimitExceeded
            }
            Error::BackendEnteredUndefinedState => {
                prismatoid_sys::PrismError::BackendEnteredUndefinedState
            }
            Error::LibraryLoadFailed => prismatoid_sys::PrismError::LibraryLoadFailed,
            Error::LibraryInvalid => prismatoid_sys::PrismError::LibraryInvalid,
            Error::IncompatibleAbi => prismatoid_sys::PrismError::IncompatibleAbi,
            Error::Unknown => prismatoid_sys::PrismError::Unknown,
        }
    }
}

pub(crate) fn check(err: prismatoid_sys::PrismError) -> Result<(), Error> {
    if err == prismatoid_sys::PrismError::Ok {
        Ok(())
    } else {
        Err(Error::from(err))
    }
}

/// Helper function to retrieve the human-readable description for a raw Prism error.
pub fn error_string(err: prismatoid_sys::PrismError) -> &'static str {
    unsafe {
        let ptr = prismatoid_sys::prism_error_string(err);
        if ptr.is_null() {
            "Unknown error"
        } else {
            CStr::from_ptr(ptr).to_str().unwrap_or("Unknown error")
        }
    }
}
