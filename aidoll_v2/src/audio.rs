use anyhow::Result;

/// Trait for reading audio input (mic).
pub trait AudioInput: Send {
    fn read_chunk(&mut self) -> Result<Vec<i16>>;
}

/// Trait for writing audio output (speaker).
pub trait AudioOutput: Send {
    fn write_chunk(&mut self, pcm: &[i16]) -> Result<()>;
}

/// Convert stereo interleaved samples to mono (take left channel).
pub fn stereo_to_mono(stereo: &[i16]) -> Vec<i16> {
    stereo.chunks(2).map(|pair| pair[0]).collect()
}

/// Convert mono samples to stereo (duplicate each sample).
pub fn mono_to_stereo(mono: &[i16]) -> Vec<i16> {
    let mut stereo = Vec::with_capacity(mono.len() * 2);
    for &sample in mono {
        stereo.push(sample);
        stereo.push(sample);
    }
    stereo
}

/// Calculate peak amplitude of a sample buffer.
pub fn peak_amplitude(samples: &[i16]) -> i16 {
    samples.iter().map(|s| s.abs()).max().unwrap_or(0)
}
