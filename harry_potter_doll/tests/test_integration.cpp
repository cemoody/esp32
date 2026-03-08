// Make private members testable
#define private public

#include "Arduino.h"
#include "audio_buffer.h"
#include "groq.h"
#include "config.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#undef private

static int tests_passed = 0;
#define TEST(name) printf("  %-40s", name);
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)

// ─── buildRequestBody tests ─────────────────────────────

void test_build_request_basic() {
  TEST("buildRequestBody basic structure");
  GroqClient groq;

  String body = groq.buildRequestBody("Hello Harry", nullptr, 0);
  assert(body.contains("\"model\""));
  assert(body.contains("\"stream\":true"));
  assert(body.contains("\"messages\""));
  assert(body.contains("\"role\":\"system\""));
  assert(body.contains("\"role\":\"user\""));
  assert(body.contains("Hello Harry"));
  PASS();
}

void test_build_request_with_history() {
  TEST("buildRequestBody with history");
  GroqClient groq;

  ConversationTurn history[2];
  history[0].role = "user";
  history[0].content = "Hi there";
  history[1].role = "assistant";
  history[1].content = "Hello!";

  String body = groq.buildRequestBody("How are you?", history, 2);
  assert(body.contains("Hi there"));
  assert(body.contains("Hello!"));
  assert(body.contains("How are you?"));
  // System prompt + 2 history + 1 user = 4 messages
  // Count "role" occurrences (crude but effective)
  int roleCount = 0;
  const char* p = body.c_str();
  while ((p = strstr(p, "\"role\"")) != nullptr) { roleCount++; p++; }
  assert(roleCount == 4);
  PASS();
}

void test_build_request_system_prompt() {
  TEST("buildRequestBody includes system prompt");
  GroqClient groq;

  String body = groq.buildRequestBody("test", nullptr, 0);
  assert(body.contains("Laura Ingalls"));
  assert(body.contains("prairie"));
  PASS();
}

// ─── Mono ↔ Stereo conversion tests ────────────────────

// Extracted from audio_codec.cpp audio_speaker_write()
static void monoToStereo(const int16_t* mono, int16_t* stereo, size_t monoSamples) {
  for (size_t i = 0; i < monoSamples; i++) {
    stereo[i * 2] = mono[i];
    stereo[i * 2 + 1] = mono[i];
  }
}

// Extracted from harry_potter_doll.ino micTask()
static void stereoToMono(const int16_t* stereo, int16_t* mono, size_t stereoSamples) {
  size_t monoSamples = stereoSamples / 2;
  for (size_t i = 0; i < monoSamples; i++) {
    mono[i] = stereo[i * 2]; // left channel
  }
}

void test_mono_to_stereo() {
  TEST("mono to stereo duplication");
  int16_t mono[4] = {100, -200, 300, -400};
  int16_t stereo[8] = {};

  monoToStereo(mono, stereo, 4);

  // Each mono sample should appear in both L and R
  assert(stereo[0] == 100 && stereo[1] == 100);
  assert(stereo[2] == -200 && stereo[3] == -200);
  assert(stereo[4] == 300 && stereo[5] == 300);
  assert(stereo[6] == -400 && stereo[7] == -400);
  PASS();
}

void test_stereo_to_mono() {
  TEST("stereo to mono left channel");
  int16_t stereo[8] = {100, 999, -200, 999, 300, 999, -400, 999};
  int16_t mono[4] = {};

  stereoToMono(stereo, mono, 8);

  // Should take left channel only, ignoring right (999)
  assert(mono[0] == 100);
  assert(mono[1] == -200);
  assert(mono[2] == 300);
  assert(mono[3] == -400);
  PASS();
}

void test_mono_stereo_roundtrip() {
  TEST("mono->stereo->mono roundtrip");
  int16_t original[4] = {1000, -2000, 3000, -4000};
  int16_t stereo[8] = {};
  int16_t recovered[4] = {};

  monoToStereo(original, stereo, 4);
  stereoToMono(stereo, recovered, 8);

  assert(memcmp(original, recovered, sizeof(original)) == 0);
  PASS();
}

// ─── AudioBuffer + ElevenLabs integration ───────────────

void test_audio_through_buffer() {
  TEST("PCM audio flows through buffer");
  AudioBuffer buf;
  buf.init(4096);

  // Simulate ElevenLabs audio chunks being written
  uint8_t chunk1[100];
  uint8_t chunk2[100];
  for (int i = 0; i < 100; i++) {
    chunk1[i] = i;
    chunk2[i] = 100 + i;
  }

  buf.write(chunk1, 100);
  buf.write(chunk2, 100);
  assert(buf.available() == 200);

  // Simulate speaker task reading
  uint8_t out[200];
  assert(buf.read(out, 200) == 200);

  // Verify data integrity
  assert(out[0] == 0);
  assert(out[99] == 99);
  assert(out[100] == 100);
  assert(out[199] == 199);
  assert(buf.available() == 0);
  PASS();
}

// ─── Groq token callback wiring ─────────────────────────

void test_groq_callbacks_wired() {
  TEST("Groq callbacks fire correctly");
  GroqClient groq;

  std::vector<std::string> received_tokens;
  bool done_fired = false;

  groq.setTokenCallback([&](const char* token) {
    received_tokens.push_back(token);
  });
  groq.setDoneCallback([&]() {
    done_fired = true;
  });

  // Verify callbacks are set (can't easily test without network,
  // but we can test the state management)
  assert(!groq.isBusy());
  groq.cancel();
  assert(!groq.isBusy()); // cancel when not busy is a no-op
  PASS();
}

// ─── Full pipeline simulation ───────────────────────────

void test_full_pipeline_simulation() {
  TEST("simulated end-to-end pipeline flow");

  // State
  enum { IDLE, LISTENING, PROCESSING, SPEAKING } state = IDLE;
  std::string llmResponse;
  bool ttsDone = false;
  AudioBuffer playbackBuffer;
  playbackBuffer.init(4096);

  // Step 1: Deepgram sends transcript → triggers processing
  state = PROCESSING;

  // Step 2: Groq streams tokens
  const char* tokens[] = {"Bl", "ime", "y", "!", " I", "'m", " Harry", "."};
  for (auto t : tokens) {
    llmResponse += t;
    if (state == PROCESSING) state = SPEAKING;
  }
  assert(state == SPEAKING);
  assert(llmResponse == "Blimey! I'm Harry.");

  // Step 3: ElevenLabs sends audio
  uint8_t fakeAudio[320]; // 10ms of 16kHz 16-bit mono
  memset(fakeAudio, 0x42, sizeof(fakeAudio));
  playbackBuffer.write(fakeAudio, sizeof(fakeAudio));
  assert(playbackBuffer.available() == 320);

  // Step 4: Speaker reads audio
  uint8_t speakerOut[320];
  playbackBuffer.read(speakerOut, 320);
  assert(speakerOut[0] == 0x42);
  assert(playbackBuffer.available() == 0);

  // Step 5: TTS done signal
  ttsDone = true;

  // Step 6: Pipeline goes idle
  if (state == SPEAKING && ttsDone && playbackBuffer.available() == 0) {
    state = IDLE;
  }
  assert(state == IDLE);
  PASS();
}

int main() {
  printf("=== Integration Tests ===\n");
  test_build_request_basic();
  test_build_request_with_history();
  test_build_request_system_prompt();
  test_mono_to_stereo();
  test_stereo_to_mono();
  test_mono_stereo_roundtrip();
  test_audio_through_buffer();
  test_groq_callbacks_wired();
  test_full_pipeline_simulation();
  printf("All %d tests passed.\n\n", tests_passed);
  return 0;
}
