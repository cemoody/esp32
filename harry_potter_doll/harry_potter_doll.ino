#include <WiFi.h>
#include <esp_wifi.h>
#include "config.h"
#include "audio_codec.h"
#include "audio_buffer.h"
#include "deepgram.h"
#include "groq.h"
#include "elevenlabs.h"
#include "rgb_feedback.h"

// ─── Global State ───────────────────────────────────────
enum PipelineState {
  PIPE_IDLE,
  PIPE_LISTENING,
  PIPE_PROCESSING,
  PIPE_SPEAKING
};

static volatile PipelineState pipelineState = PIPE_IDLE;
static volatile bool interruptRequested = false;
static volatile bool ttsDone = false;

// Sentence buffer for streaming TTS in sentence-sized chunks
static String sentenceBuffer;

// Audio playback buffer
static AudioBuffer playbackBuffer;

// API clients
static DeepgramClient deepgram;
static GroqClient groq;
static ElevenLabsClient elevenlabs;

// Conversation history
static ConversationTurn conversationHistory[MAX_CONVERSATION_TURNS];
static size_t historyCount = 0;

// Accumulated transcript for current utterance
static String currentTranscript;

// Accumulated LLM response
static String currentResponse;

// ─── Conversation History Management ────────────────────
static void addToHistory(const char* role, const char* content) {
  if (historyCount >= MAX_CONVERSATION_TURNS) {
    // Shift history, drop oldest
    for (size_t i = 0; i < MAX_CONVERSATION_TURNS - 1; i++) {
      conversationHistory[i] = conversationHistory[i + 1];
    }
    historyCount = MAX_CONVERSATION_TURNS - 1;
  }
  conversationHistory[historyCount].role = role;
  conversationHistory[historyCount].content = content;
  historyCount++;
}

// ─── Pipeline Interruption ──────────────────────────────
static void interruptPipeline() {
  Serial.println("[PIPE] Interruption!");
  interruptRequested = true;
  groq.cancel();
  elevenlabs.disconnect();
  playbackBuffer.flush();
  pipelineState = PIPE_LISTENING;
  rgb_set_state(STATE_LISTENING);
}

// ─── Deepgram Callbacks ─────────────────────────────────
static void onTranscript(const char* transcript) {
  Serial.printf("[PIPE] Transcript: %s\n", transcript);

  // If we're speaking and get a transcript, interrupt
  if (pipelineState == PIPE_SPEAKING) {
    interruptPipeline();
  }

  currentTranscript += transcript;
  currentTranscript += " ";

  // Start LLM processing
  if (currentTranscript.length() > 0) {
    pipelineState = PIPE_PROCESSING;
    rgb_set_state(STATE_THINKING);

    String trimmed = currentTranscript;
    trimmed.trim();
    Serial.printf("[PIPE] Sending to LLM: %s\n", trimmed.c_str());

    // Reset for next utterance
    String userMsg = trimmed;
    currentTranscript = "";
    currentResponse = "";
    sentenceBuffer = "";

    // Connect ElevenLabs for streaming TTS
    if (!elevenlabs.isConnected()) {
      elevenlabs.connect();
    }

    // Send to Groq (this blocks on the pipeline task until done)
    addToHistory("user", userMsg.c_str());
    bool groqOk = groq.requestCompletion(userMsg.c_str(), conversationHistory, historyCount - 1);

    // If Groq failed, recover to IDLE
    if (!groqOk && pipelineState == PIPE_PROCESSING) {
      Serial.println("[PIPE] Groq failed, returning to IDLE");
      elevenlabs.disconnect();
      pipelineState = PIPE_IDLE;
      rgb_set_state(STATE_IDLE);
    }
  }
}

static void onPartialTranscript(const char* transcript) {
  // Switch to listening state when we hear speech
  if (pipelineState == PIPE_IDLE) {
    pipelineState = PIPE_LISTENING;
    rgb_set_state(STATE_LISTENING);
  }
}

// ─── Groq Callbacks ─────────────────────────────────────
static void onToken(const char* token) {
  if (interruptRequested) return;

  Serial.print(token);
  currentResponse += token;

  // Switch to speaking state once we start getting tokens
  if (pipelineState == PIPE_PROCESSING) {
    pipelineState = PIPE_SPEAKING;
    rgb_set_state(STATE_SPEAKING);
  }
}

static void onGroqDone() {
  if (interruptRequested) {
    interruptRequested = false;
    return;
  }

  Serial.printf("\n[PIPE] LLM done, response: %s\n", currentResponse.c_str());

  // Send full response to ElevenLabs at once, then flush
  if (elevenlabs.isConnected() && currentResponse.length() > 0) {
    ttsDone = false;
    elevenlabs.sendText(currentResponse.c_str());
    elevenlabs.flush();
    Serial.printf("[PIPE] Sent %d chars to EL\n", currentResponse.length());
  }

  // Save assistant response to history
  if (currentResponse.length() > 0) {
    addToHistory("assistant", currentResponse.c_str());
  }
}

// ─── ElevenLabs Callbacks ───────────────────────────────
static uint32_t totalAudioBytes = 0;

static void onAudioChunk(const uint8_t* data, size_t len) {
  if (interruptRequested) return;

  totalAudioBytes += len;
  // Write to playback buffer (speaker task reads from this)
  size_t written = playbackBuffer.write(data, len);
  Serial.printf("[AUD] chunk=%d written=%d total=%u buf=%d\n",
    len, written, totalAudioBytes, playbackBuffer.available());
}

static void onTTSDone() {
  Serial.println("[PIPE] TTS done");
  ttsDone = true;
}

// ─── FreeRTOS Tasks ─────────────────────────────────────

// Mic Task: captures audio from I2S, converts stereo→mono, sends to Deepgram
static void micTask(void* param) {
  // Buffer for stereo mic data (2 channels × 16-bit)
  uint8_t stereoBuffer[MIC_CHUNK_SIZE * 2];
  int16_t monoBuffer[MIC_CHUNK_SIZE / 2];

  Serial.println("[MIC] Task started");

  uint32_t micDbgCount = 0;
  int16_t peakLevel = 0;

  while (true) {
    if (!deepgram.isConnected()) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Read stereo data from mic
    size_t bytesRead = audio_mic_read(stereoBuffer, sizeof(stereoBuffer));
    if (bytesRead == 0) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    // Convert stereo to mono (take left channel only)
    int16_t* stereo = (int16_t*)stereoBuffer;
    size_t stereoSamples = bytesRead / 2;  // Total samples (L+R)
    size_t monoSamples = stereoSamples / 2;

    for (size_t i = 0; i < monoSamples; i++) {
      monoBuffer[i] = stereo[i * 2];  // Left channel
    }

    // Track peak level for debug
    for (size_t i = 0; i < monoSamples; i++) {
      int16_t v = monoBuffer[i] < 0 ? -monoBuffer[i] : monoBuffer[i];
      if (v > peakLevel) peakLevel = v;
    }
    micDbgCount++;
    if (micDbgCount % 50 == 0) {  // ~every 1 second
      Serial.printf("[MIC] peak=%d sent=%lu bytes=%d\n", peakLevel, micDbgCount, monoSamples * 2);
      peakLevel = 0;
    }

    // During playback, only send audio if peak is above interrupt threshold
    // This prevents speaker bleed from triggering Deepgram, but allows
    // loud human speech to interrupt
    if (pipelineState == PIPE_SPEAKING && peakLevel <= INTERRUPT_PEAK_THRESHOLD) {
      vTaskDelay(pdMS_TO_TICKS(MIC_SEND_INTERVAL_MS));
      continue;
    }

    // Send mono audio to Deepgram
    deepgram.sendAudio((uint8_t*)monoBuffer, monoSamples * 2);

    vTaskDelay(pdMS_TO_TICKS(MIC_SEND_INTERVAL_MS));
  }
}

// Pipeline Task: manages Deepgram WS, triggers Groq, manages ElevenLabs
static void pipelineTask(void* param) {
  Serial.println("[PIPE] Task started");

  while (true) {
    // Poll Deepgram WebSocket for transcripts
    deepgram.poll();

    // Poll ElevenLabs WebSocket for audio chunks
    elevenlabs.poll();

    // If EL disconnected unexpectedly during speaking, treat as TTS done
    if (pipelineState == PIPE_SPEAKING && !elevenlabs.isConnected() && !ttsDone) {
      Serial.println("[PIPE] EL disconnected, forcing ttsDone");
      ttsDone = true;
    }

    // Check if playback is done and we should go idle
    // Wait for TTS to finish AND playback buffer to drain
    if (pipelineState == PIPE_SPEAKING &&
        !groq.isBusy() &&
        ttsDone &&
        playbackBuffer.available() == 0) {
      Serial.println("[PIPE] Playback complete, going idle");
      pipelineState = PIPE_IDLE;
      rgb_set_state(STATE_IDLE);

      // Keep ElevenLabs connected for faster next turn
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// Speaker Task: reads from playback buffer, writes PCM to I2S
static void speakerTask(void* param) {
  Serial.println("[SPK] Task started");

  // Silence buffer for when there's nothing to play
  uint8_t silence[256];
  memset(silence, 0, sizeof(silence));

  uint8_t pcmChunk[512];

  while (true) {
    size_t available = playbackBuffer.available();

    if (available >= sizeof(pcmChunk)) {
      size_t bytesRead = playbackBuffer.read(pcmChunk, sizeof(pcmChunk));
      if (bytesRead > 0) {
        audio_speaker_write(pcmChunk, bytesRead);
      }
    } else if (available > 0) {
      // Read whatever is available
      size_t bytesRead = playbackBuffer.read(pcmChunk, available);
      if (bytesRead > 0) {
        audio_speaker_write(pcmChunk, bytesRead);
      }
    } else {
      // Write silence to keep I2S clock running
      audio_speaker_write(silence, sizeof(silence));
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// LED Task: updates RGB animation
static void ledTask(void* param) {
  Serial.println("[LED] Task started");
  while (true) {
    rgb_update();
    vTaskDelay(pdMS_TO_TICKS(33));  // ~30 FPS
  }
}

// ─── WiFi Connection ────────────────────────────────────
static bool connectWiFi() {
  Serial.printf("[WIFI] Connecting to %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Disable WiFi power save for low latency
  esp_wifi_set_ps(WIFI_PS_NONE);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.println("\n[WIFI] Connection failed!");
  return false;
}

// ─── Reconnection Logic ─────────────────────────────────
static void checkConnections() {
  // WiFi reconnect
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Disconnected, reconnecting...");
    rgb_set_state(STATE_ERROR);
    WiFi.reconnect();
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) return;
  }

  // Deepgram reconnect
  if (!deepgram.isConnected()) {
    Serial.println("[DG] Reconnecting...");
    deepgram.connect();
  }
}

// ─── Setup ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n═══════════════════════════════════════");
  Serial.println("  Harry Potter AI Doll");
  Serial.println("  Waveshare ESP32-S3-AUDIO-Board");
  Serial.println("═══════════════════════════════════════\n");

  // Init RGB LEDs first for visual feedback
  rgb_init();
  rgb_set_state(STATE_CONNECTING);
  rgb_update();

  // Init audio hardware
  if (!audio_codec_init()) {
    Serial.println("[INIT] Audio codec init FAILED!");
    rgb_set_state(STATE_ERROR);
    while (true) { rgb_update(); delay(33); }
  }
  Serial.println("[INIT] Audio codec ready");

  // Init playback buffer
  if (!playbackBuffer.init(PLAYBACK_BUFFER_SIZE)) {
    Serial.println("[INIT] Playback buffer init FAILED!");
    rgb_set_state(STATE_ERROR);
    while (true) { rgb_update(); delay(33); }
  }
  Serial.println("[INIT] Playback buffer ready");

  // Connect WiFi
  if (!connectWiFi()) {
    rgb_set_state(STATE_ERROR);
    while (true) { rgb_update(); delay(33); }
  }

  // Set up API callbacks
  deepgram.setTranscriptCallback(onTranscript);
  deepgram.setPartialCallback(onPartialTranscript);
  groq.setTokenCallback(onToken);
  groq.setDoneCallback(onGroqDone);
  groq.setPollCallback([]() { elevenlabs.poll(); });
  elevenlabs.setAudioCallback(onAudioChunk);
  elevenlabs.setDoneCallback(onTTSDone);

  // Connect to Deepgram
  if (!deepgram.connect()) {
    Serial.println("[INIT] Deepgram connection failed, will retry...");
  }

  // Launch FreeRTOS tasks
  xTaskCreatePinnedToCore(micTask, "mic", 8192, nullptr, 5, nullptr, 0);
  xTaskCreatePinnedToCore(pipelineTask, "pipeline", 16384, nullptr, 4, nullptr, 1);
  xTaskCreatePinnedToCore(speakerTask, "speaker", 4096, nullptr, 5, nullptr, 0);
  xTaskCreatePinnedToCore(ledTask, "led", 2048, nullptr, 1, nullptr, 0);

  // Ready!
  pipelineState = PIPE_IDLE;
  rgb_set_state(STATE_IDLE);
  Serial.println("\n[INIT] Ready! Say \"Hello Harry\" to start chatting.\n");
}

// ─── Loop ───────────────────────────────────────────────
void loop() {
  // Main loop handles reconnection and watchdog
  static uint32_t lastCheck = 0;

  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    checkConnections();
    Serial.printf("[DBG] up=%lus dg=%d pipe=%d heap=%u\n",
      millis() / 1000,
      deepgram.isConnected() ? 1 : 0,
      (int)pipelineState,
      esp_get_free_heap_size());
  }

  delay(100);
}
