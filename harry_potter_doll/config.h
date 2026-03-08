#pragma once

#include "secrets.h"  // WIFI_SSID, WIFI_PASSWORD, DEEPGRAM_API_KEY, GROQ_API_KEY

// ─── Deepgram (Streaming STT) ──────────────────────────
#define DEEPGRAM_HOST    "api.deepgram.com"
#define DEEPGRAM_PATH    "/v1/listen?encoding=linear16&sample_rate=16000&channels=1&model=nova-3&punctuate=true&endpointing=300"

// ─── Groq (Streaming LLM) ──────────────────────────────
#define GROQ_HOST     "api.groq.com"
#define GROQ_MODEL    "meta-llama/llama-4-scout-17b-16e-instruct"
#define GROQ_MAX_TOKENS 80

// ─── TTS (Deepgram Aura via WebSocket) ───────────────
#define TTS_MODEL "aura-2-luna-en"
// Uses DEEPGRAM_API_KEY for auth (same key as STT)

// ─── Character Persona ─────────────────────────────────
// Auto-generated from laura.md — run ./generate_prompt.sh to update
#include "laura_prompt.h"
#define CHARACTER_NAME "Laura Ingalls"
#define SYSTEM_PROMPT LAURA_PROMPT

// ─── Conversation History ───────────────────────────────
#define MAX_CONVERSATION_TURNS 10  // Keep last N turns for context

// ─── Audio Hardware (Waveshare ESP32-S3-AUDIO-Board) ───
#define I2C_SDA_PIN   11
#define I2C_SCL_PIN   10
#define I2S_MCLK_PIN  12
#define I2S_SCLK_PIN  13
#define I2S_LRCK_PIN  14
#define I2S_DSIN_PIN  15  // Mic data in (from ES7210)
#define I2S_DOUT_PIN  16  // Speaker data out (to ES8311)

#define AUDIO_SAMPLE_RATE  16000
#define AUDIO_MCLK_MULTIPLE 256

// ES8311 speaker codec
#define ES8311_ADDR    0x18
#define SPEAKER_VOLUME 70   // 0-100, capped for child safety

// ES7210 mic codec
#define ES7210_ADDR    0x40
#define MIC_GAIN       ES7210_MIC_GAIN_37_5DB

// TCA9555 IO expander
#define TCA9555_ADDR   0x20
#define PA_AMP_PIN     8    // TCA9555 EXIO8 — PA amplifier enable

// ─── RGB LEDs (WS2812 NeoPixel) ────────────────────────
#define RGB_LED_PIN    38
#define RGB_LED_COUNT  7
#define RGB_BRIGHTNESS 40   // Keep dim for nighttime use

// ─── Audio Buffer ───────────────────────────────────────
#define PLAYBACK_BUFFER_SIZE (16000 * 2 * 30)  // 30 seconds of 16kHz 16-bit mono (960KB, fits in PSRAM)
#define MIC_CHUNK_SIZE       512          // Bytes per mic read (256 samples)
#define MIC_SEND_INTERVAL_MS 20           // Send mic data every 20ms
#define INTERRUPT_PEAK_THRESHOLD 5000     // Mic peak above this triggers interrupt during playback
