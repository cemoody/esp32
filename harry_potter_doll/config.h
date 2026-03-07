#pragma once

// ─── WiFi ───────────────────────────────────────────────
#define WIFI_SSID     "REDACTED"
#define WIFI_PASSWORD "REDACTED"

// ─── Deepgram (Streaming STT) ──────────────────────────
#define DEEPGRAM_API_KEY "REDACTED"
#define DEEPGRAM_HOST    "api.deepgram.com"
#define DEEPGRAM_PATH    "/v1/listen?encoding=linear16&sample_rate=16000&channels=1&model=nova-3&punctuate=true&endpointing=300"

// ─── Groq (Streaming LLM) ──────────────────────────────
#define GROQ_API_KEY  "REDACTED"
#define GROQ_HOST     "api.groq.com"
#define GROQ_MODEL    "llama-3.3-70b-versatile"
#define GROQ_MAX_TOKENS 150

// ─── ElevenLabs (Streaming TTS) ────────────────────────
#define ELEVENLABS_API_KEY "REDACTED"
#define ELEVENLABS_VOICE_ID "6jqGBPwwAbehMvDK7PmB"  // "harry potted" — custom Harry Potter voice
#define ELEVENLABS_MODEL    "eleven_turbo_v2_5"
#define ELEVENLABS_HOST     "api.elevenlabs.io"

// ─── Character Persona ─────────────────────────────────
#define CHARACTER_NAME "Harry Potter"
#define SYSTEM_PROMPT \
  "You are Harry Potter, an 11-year-old wizard who just started at Hogwarts School of Witchcraft and Wizardry. " \
  "You speak like a British kid — friendly, curious, and sometimes a bit nervous. " \
  "You love talking about Quidditch, your friends Ron and Hermione, your owl Hedwig, and your adventures at Hogwarts. " \
  "You're amazed by magic since you grew up with the boring Dursleys. " \
  "Keep your responses short — 1 to 3 sentences, like a real kid talking. " \
  "Use simple words. Never break character. If someone asks something you wouldn't know as Harry, " \
  "make up a fun magical answer that fits the wizarding world."

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
