#pragma once

#include <stdint.h>

enum DollState {
  STATE_IDLE,       // Golden pulse (Gryffindor)
  STATE_LISTENING,  // Blue breathing (child speaking)
  STATE_THINKING,   // Purple spinning (LLM generating)
  STATE_SPEAKING,   // Green pulse (TTS playing)
  STATE_ERROR,      // Red flash
  STATE_CONNECTING  // White fade (WiFi/init)
};

// Initialize NeoPixel LEDs
void rgb_init();

// Set the current state (changes LED animation)
void rgb_set_state(DollState state);

// Update LED animation (call from loop/task, ~30fps)
void rgb_update();
