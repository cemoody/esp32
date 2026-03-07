#include <cassert>
#include <cstdio>

static int tests_passed = 0;
#define TEST(name) printf("  %-40s", name);
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)

// Replicate pipeline state types from harry_potter_doll.ino
enum PipelineState {
  PIPE_IDLE,
  PIPE_LISTENING,
  PIPE_PROCESSING,
  PIPE_SPEAKING
};

enum DollState {
  STATE_IDLE,
  STATE_LISTENING,
  STATE_THINKING,
  STATE_SPEAKING,
  STATE_ERROR,
  STATE_CONNECTING
};

// Simulated pipeline state
static PipelineState pipelineState;
static bool interruptRequested;
static bool ttsDone;
static bool groqBusy;
static int playbackAvailable;
static DollState ledState;

static void rgb_set_state(DollState s) { ledState = s; }

// Transition functions extracted from pipeline logic
static void simulatePartialTranscript() {
  if (pipelineState == PIPE_IDLE) {
    pipelineState = PIPE_LISTENING;
    rgb_set_state(STATE_LISTENING);
  }
}

static void simulateTranscriptReceived() {
  pipelineState = PIPE_PROCESSING;
  rgb_set_state(STATE_THINKING);
}

static void simulateTokenReceived() {
  if (pipelineState == PIPE_PROCESSING) {
    pipelineState = PIPE_SPEAKING;
    rgb_set_state(STATE_SPEAKING);
  }
}

static void simulateGroqDone() {
  groqBusy = false;
  ttsDone = false; // will be set by TTS completion
}

static void simulateTTSDone() {
  ttsDone = true;
}

static void simulatePlaybackCheck() {
  if (pipelineState == PIPE_SPEAKING &&
      !groqBusy &&
      ttsDone &&
      playbackAvailable == 0) {
    pipelineState = PIPE_IDLE;
    rgb_set_state(STATE_IDLE);
  }
}

static void simulateGroqFailure() {
  groqBusy = false;
  if (pipelineState == PIPE_PROCESSING) {
    pipelineState = PIPE_IDLE;
    rgb_set_state(STATE_IDLE);
  }
}

static void simulateInterrupt() {
  interruptRequested = true;
  playbackAvailable = 0;
  pipelineState = PIPE_LISTENING;
  rgb_set_state(STATE_LISTENING);
}

void test_idle_to_listening() {
  TEST("IDLE -> LISTENING on speech");
  pipelineState = PIPE_IDLE;
  simulatePartialTranscript();
  assert(pipelineState == PIPE_LISTENING);
  assert(ledState == STATE_LISTENING);
  PASS();
}

void test_listening_to_processing() {
  TEST("LISTENING -> PROCESSING on transcript");
  pipelineState = PIPE_LISTENING;
  simulateTranscriptReceived();
  assert(pipelineState == PIPE_PROCESSING);
  assert(ledState == STATE_THINKING);
  PASS();
}

void test_processing_to_speaking() {
  TEST("PROCESSING -> SPEAKING on first token");
  pipelineState = PIPE_PROCESSING;
  simulateTokenReceived();
  assert(pipelineState == PIPE_SPEAKING);
  assert(ledState == STATE_SPEAKING);
  PASS();
}

void test_speaking_to_idle() {
  TEST("SPEAKING -> IDLE when TTS done + buffer empty");
  pipelineState = PIPE_SPEAKING;
  groqBusy = false;
  ttsDone = true;
  playbackAvailable = 0;
  simulatePlaybackCheck();
  assert(pipelineState == PIPE_IDLE);
  assert(ledState == STATE_IDLE);
  PASS();
}

void test_speaking_waits_for_tts() {
  TEST("SPEAKING stays if TTS not done");
  pipelineState = PIPE_SPEAKING;
  groqBusy = false;
  ttsDone = false;
  playbackAvailable = 0;
  simulatePlaybackCheck();
  assert(pipelineState == PIPE_SPEAKING); // should NOT go idle
  PASS();
}

void test_speaking_waits_for_buffer() {
  TEST("SPEAKING stays if buffer has data");
  pipelineState = PIPE_SPEAKING;
  groqBusy = false;
  ttsDone = true;
  playbackAvailable = 100;
  simulatePlaybackCheck();
  assert(pipelineState == PIPE_SPEAKING);
  PASS();
}

void test_groq_failure_recovery() {
  TEST("Groq failure -> IDLE from PROCESSING");
  pipelineState = PIPE_PROCESSING;
  groqBusy = true;
  simulateGroqFailure();
  assert(pipelineState == PIPE_IDLE);
  assert(ledState == STATE_IDLE);
  PASS();
}

void test_interrupt_during_speaking() {
  TEST("interrupt during SPEAKING -> LISTENING");
  pipelineState = PIPE_SPEAKING;
  playbackAvailable = 5000;
  simulateInterrupt();
  assert(pipelineState == PIPE_LISTENING);
  assert(ledState == STATE_LISTENING);
  assert(interruptRequested == true);
  PASS();
}

void test_full_conversation_cycle() {
  TEST("full cycle: IDLE->...->IDLE");
  pipelineState = PIPE_IDLE;
  groqBusy = false;
  ttsDone = false;
  playbackAvailable = 0;
  interruptRequested = false;

  // User starts speaking
  simulatePartialTranscript();
  assert(pipelineState == PIPE_LISTENING);

  // Transcript received
  simulateTranscriptReceived();
  assert(pipelineState == PIPE_PROCESSING);
  groqBusy = true;

  // First token arrives
  simulateTokenReceived();
  assert(pipelineState == PIPE_SPEAKING);

  // More tokens... still speaking
  simulateTokenReceived();
  assert(pipelineState == PIPE_SPEAKING);

  // Groq done, TTS starts
  simulateGroqDone();
  assert(!groqBusy);

  // Buffer has audio
  playbackAvailable = 1000;
  simulatePlaybackCheck();
  assert(pipelineState == PIPE_SPEAKING); // still playing

  // TTS finishes
  simulateTTSDone();
  assert(ttsDone);

  // Buffer drains
  playbackAvailable = 0;
  simulatePlaybackCheck();
  assert(pipelineState == PIPE_IDLE);
  assert(ledState == STATE_IDLE);
  PASS();
}

int main() {
  printf("=== Pipeline State Machine Tests ===\n");
  test_idle_to_listening();
  test_listening_to_processing();
  test_processing_to_speaking();
  test_speaking_to_idle();
  test_speaking_waits_for_tts();
  test_speaking_waits_for_buffer();
  test_groq_failure_recovery();
  test_interrupt_during_speaking();
  test_full_conversation_cycle();
  printf("All %d tests passed.\n\n", tests_passed);
  return 0;
}
