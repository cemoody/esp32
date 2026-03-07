#pragma once

#include <stdint.h>
#include <stddef.h>

// Initialize I2C bus, TCA9555, ES8311 (speaker), ES7210 (mic), and I2S
bool audio_codec_init();

// Write PCM samples to speaker (blocking)
// data: 16-bit signed mono PCM at 16kHz
// len: number of bytes
size_t audio_speaker_write(const uint8_t* data, size_t len);

// Read PCM samples from mic (blocking)
// data: buffer for 16-bit signed stereo PCM at 16kHz
// len: max bytes to read
// Returns bytes actually read
size_t audio_mic_read(uint8_t* data, size_t len);

// Set speaker volume (0-100)
void audio_set_volume(uint8_t volume);

// Enable/disable PA amplifier
void audio_pa_enable(bool enable);
