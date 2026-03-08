#include "Arduino.h"
#include <cassert>
#include <cstdio>
#include <cstdint>

static int tests_passed = 0;
static int tests_failed = 0;
#define TEST(name) printf("  %-50s", name);
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("FAIL: %s\n", msg); } while(0)

// ─── Pipeline state (mirrors .ino) ──────────────────────
enum PipelineState { PIPE_IDLE, PIPE_LISTENING, PIPE_PROCESSING, PIPE_SPEAKING };

static PipelineState pipelineState;
static bool micShouldSend;  // whether mic task would send audio to Deepgram

// Interrupt threshold: peak amplitude above which we consider it
// a real human interrupt vs just speaker bleed
#define INTERRUPT_PEAK_THRESHOLD 5000

// ─── Current mic behavior ───────────────────────────────
// Fully mutes during SPEAKING — no interrupt possible
static bool currentMicPolicy(PipelineState state) {
  // Current: skip entirely during SPEAKING
  if (state == PIPE_SPEAKING) return false;
  return true;
}

// ─── Desired mic behavior ───────────────────────────────
// Always sends audio, but uses energy threshold for interrupts
static bool desiredMicPolicy(PipelineState state) {
  // Always send audio to Deepgram (even during SPEAKING)
  return true;
}

// Should an interrupt be triggered based on mic peak level?
static bool shouldInterrupt(PipelineState state, int16_t peakLevel) {
  if (state != PIPE_SPEAKING) return false;
  return peakLevel > INTERRUPT_PEAK_THRESHOLD;
}

// ─── Tests ──────────────────────────────────────────────

void test_mic_sends_during_speaking() {
  TEST("mic sends audio during SPEAKING");

  bool sends = desiredMicPolicy(PIPE_SPEAKING);
  assert(sends == true);
  PASS();
}

void test_mic_active_during_speaking() {
  TEST("mic stays active during SPEAKING");

  bool sends = desiredMicPolicy(PIPE_SPEAKING);
  assert(sends == true);
  PASS();
}

void test_mic_active_during_idle() {
  TEST("mic active during IDLE");

  assert(desiredMicPolicy(PIPE_IDLE) == true);
  PASS();
}

void test_mic_active_during_processing() {
  TEST("mic active during PROCESSING");

  assert(desiredMicPolicy(PIPE_PROCESSING) == true);
  PASS();
}

void test_loud_speech_triggers_interrupt() {
  TEST("loud speech (peak > threshold) triggers interrupt");

  // User speaks loudly over the speaker
  int16_t loudPeak = 15000;
  assert(shouldInterrupt(PIPE_SPEAKING, loudPeak) == true);
  PASS();
}

void test_quiet_bleed_no_interrupt() {
  TEST("quiet speaker bleed (peak < threshold) no interrupt");

  // Speaker audio bleeding into mic — low level
  int16_t quietPeak = 2000;
  assert(shouldInterrupt(PIPE_SPEAKING, quietPeak) == false);
  PASS();
}

void test_no_interrupt_when_idle() {
  TEST("no interrupt triggered when IDLE");

  // Even loud speech shouldn't trigger interrupt when not speaking
  assert(shouldInterrupt(PIPE_IDLE, 20000) == false);
  PASS();
}

void test_no_interrupt_when_processing() {
  TEST("no interrupt triggered when PROCESSING");

  assert(shouldInterrupt(PIPE_PROCESSING, 20000) == false);
  PASS();
}

void test_threshold_boundary() {
  TEST("peak exactly at threshold does not trigger");

  assert(shouldInterrupt(PIPE_SPEAKING, INTERRUPT_PEAK_THRESHOLD) == false);
  assert(shouldInterrupt(PIPE_SPEAKING, INTERRUPT_PEAK_THRESHOLD + 1) == true);
  PASS();
}

int main() {
  printf("=== Interrupt During Playback Tests ===\n");
  test_mic_sends_during_speaking();
  test_mic_active_during_speaking();
  test_mic_active_during_idle();
  test_mic_active_during_processing();
  test_loud_speech_triggers_interrupt();
  test_quiet_bleed_no_interrupt();
  test_no_interrupt_when_idle();
  test_no_interrupt_when_processing();
  test_threshold_boundary();
  printf("\nPassed: %d, Failed: %d\n\n", tests_passed, tests_failed);
  return tests_failed > 0 ? 1 : 0;
}
