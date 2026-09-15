// SPDX-License-Identifier: MPL-2.0

/// Hardware or synthesized audio stream format specification.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct AudioFormat {
    /// Number of audio channels (e.g. 1 for mono, 2 for stereo).
    pub channels: usize,
    /// Sample rate in Hertz (e.g. 22050, 44100, 48000).
    pub sample_rate: usize,
    /// Sample bit depth (typically 16 or 32 bits).
    pub bit_depth: usize,
}

/// An in-memory buffer containing synthesized PCM audio frames.
#[derive(Debug, Clone, PartialEq)]
pub struct AudioBuffer {
    /// Interleaved IEEE 32-bit floating-point PCM audio samples in range [-1.0, 1.0].
    pub samples: Vec<f32>,
    /// Number of audio channels.
    pub channels: usize,
    /// Sample rate in Hertz.
    pub sample_rate: usize,
}

impl AudioBuffer {
    /// Constructs a new AudioBuffer with the specified parameters.
    pub fn new(samples: Vec<f32>, channels: usize, sample_rate: usize) -> Self {
        Self {
            samples,
            channels,
            sample_rate,
        }
    }

    /// Returns the total number of multi-channel frames in this buffer.
    pub fn frames_count(&self) -> usize {
        self.samples.len().checked_div(self.channels).unwrap_or(0)
    }

    /// Returns the duration of the audio in seconds.
    pub fn duration_seconds(&self) -> f32 {
        if self.sample_rate == 0 || self.channels == 0 {
            0.0
        } else {
            self.frames_count() as f32 / self.sample_rate as f32
        }
    }

    /// Converts the 32-bit floating-point samples to 16-bit signed integer PCM.
    pub fn to_i16_pcm(&self) -> Vec<i16> {
        self.samples
            .iter()
            .map(|&s| {
                let clamped = s.clamp(-1.0, 1.0);
                if clamped >= 0.0 {
                    (clamped * 32767.0) as i16
                } else {
                    (clamped * 32768.0) as i16
                }
            })
            .collect()
    }

    /// Encodes the synthesized audio into a standard RIFF/WAVE byte buffer (16-bit PCM).
    pub fn to_wav_bytes(&self) -> Vec<u8> {
        let pcm_16 = self.to_i16_pcm();
        let data_size = (pcm_16.len() * 2) as u32;
        let file_size = 36 + data_size;
        let byte_rate = (self.sample_rate * self.channels * 2) as u32;
        let block_align = (self.channels * 2) as u16;

        let mut wav = Vec::with_capacity(44 + data_size as usize);

        // RIFF header
        wav.extend_from_slice(b"RIFF");
        wav.extend_from_slice(&file_size.to_le_bytes());
        wav.extend_from_slice(b"WAVE");

        // fmt subchunk
        wav.extend_from_slice(b"fmt ");
        wav.extend_from_slice(&16u32.to_le_bytes()); // Subchunk1Size (16 for PCM)
        wav.extend_from_slice(&1u16.to_le_bytes()); // AudioFormat (1 = PCM)
        wav.extend_from_slice(&(self.channels as u16).to_le_bytes());
        wav.extend_from_slice(&(self.sample_rate as u32).to_le_bytes());
        wav.extend_from_slice(&byte_rate.to_le_bytes());
        wav.extend_from_slice(&block_align.to_le_bytes());
        wav.extend_from_slice(&16u16.to_le_bytes()); // BitsPerSample

        // data subchunk
        wav.extend_from_slice(b"data");
        wav.extend_from_slice(&data_size.to_le_bytes());
        for sample in pcm_16 {
            wav.extend_from_slice(&sample.to_le_bytes());
        }

        wav
    }
}

/// A chunk of synthesized audio emitted during streaming synthesis.
#[derive(Debug, Clone, PartialEq)]
pub struct AudioChunk {
    /// Interleaved IEEE 32-bit floating-point PCM audio samples in range [-1.0, 1.0].
    pub samples: Vec<f32>,
    /// Number of audio channels.
    pub channels: usize,
    /// Sample rate in Hertz.
    pub sample_rate: usize,
}

impl AudioChunk {
    /// Constructs a new `AudioChunk` with the specified parameters.
    pub fn new(samples: Vec<f32>, channels: usize, sample_rate: usize) -> Self {
        Self {
            samples,
            channels,
            sample_rate,
        }
    }

    /// Returns the total number of multi-channel frames in this chunk.
    pub fn frames_count(&self) -> usize {
        self.samples.len().checked_div(self.channels).unwrap_or(0)
    }

    /// Returns the duration of the audio in seconds.
    pub fn duration_seconds(&self) -> f32 {
        if self.sample_rate == 0 || self.channels == 0 {
            0.0
        } else {
            self.frames_count() as f32 / self.sample_rate as f32
        }
    }

    /// Converts the 32-bit floating-point samples to 16-bit signed integer PCM.
    pub fn to_i16_pcm(&self) -> Vec<i16> {
        self.samples
            .iter()
            .map(|&s| {
                let clamped = s.clamp(-1.0, 1.0);
                if clamped >= 0.0 {
                    (clamped * 32767.0) as i16
                } else {
                    (clamped * 32768.0) as i16
                }
            })
            .collect()
    }
}

/// An iterator over chunks of synthesized audio.
#[derive(Debug, Clone)]
pub struct ChunkIterator {
    iter: std::vec::IntoIter<AudioChunk>,
}

impl ChunkIterator {
    /// Constructs a new `ChunkIterator` from a vector of audio chunks.
    pub fn new(chunks: Vec<AudioChunk>) -> Self {
        Self {
            iter: chunks.into_iter(),
        }
    }

    /// Returns `true` if the iterator contains no chunks.
    pub fn is_empty(&self) -> bool {
        self.iter.len() == 0
    }
}

impl Iterator for ChunkIterator {
    type Item = AudioChunk;

    fn next(&mut self) -> Option<Self::Item> {
        self.iter.next()
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        self.iter.size_hint()
    }
}

impl ExactSizeIterator for ChunkIterator {
    fn len(&self) -> usize {
        self.iter.len()
    }
}
