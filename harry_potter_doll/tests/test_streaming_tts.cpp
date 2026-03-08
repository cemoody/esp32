#include "Arduino.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

static int tests_passed = 0;
static int tests_failed = 0;
#define TEST(name) printf("  %-50s", name);
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("FAIL: %s\n", msg); } while(0)

struct TTSCall {
  std::string text;
  enum Type { SEND_TEXT, FLUSH } type;
};

static std::vector<TTSCall> ttsLog;
static bool elConnected = false;
static std::string currentResponse;
static std::string sentenceBuffer;

static void mockEL_sendText(const char* text) {
  ttsLog.push_back({text, TTSCall::SEND_TEXT});
}
static void mockEL_flush() {
  ttsLog.push_back({"", TTSCall::FLUSH});
}

// Sentence-chunked streaming: sends to EL on sentence boundaries
static void onToken(const char* token) {
  currentResponse += token;
  sentenceBuffer += token;

  if (elConnected) {
    size_t len = sentenceBuffer.size();
    if (len > 0) {
      char last = sentenceBuffer[len - 1];
      if (last == '.' || last == '!' || last == '?') {
        mockEL_sendText(sentenceBuffer.c_str());
        sentenceBuffer.clear();
      }
    }
  }
}

static void onGroqDone() {
  if (elConnected && !currentResponse.empty()) {
    if (!sentenceBuffer.empty()) {
      mockEL_sendText(sentenceBuffer.c_str());
      sentenceBuffer.clear();
    }
    mockEL_flush();
  }
}

static void reset() {
  ttsLog.clear();
  currentResponse.clear();
  sentenceBuffer.clear();
  elConnected = true;
}

// ─── Tests ──────────────────────────────────────────────

void test_sentence_chunks_sent() {
  TEST("sentences sent as chunks on . ! ?");
  reset();

  // Simulate tokens for: "Hello there. How are you!"
  const char* tokens[] = {"Hello", " there", ".", " How", " are", " you", "!"};
  for (auto t : tokens) onToken(t);
  onGroqDone();

  // Should be 2 SEND_TEXT (one per sentence) + 1 FLUSH
  assert(ttsLog.size() == 3);
  assert(ttsLog[0].type == TTSCall::SEND_TEXT);
  assert(ttsLog[0].text == "Hello there.");
  assert(ttsLog[1].type == TTSCall::SEND_TEXT);
  assert(ttsLog[1].text == " How are you!");
  assert(ttsLog[2].type == TTSCall::FLUSH);
  PASS();
}

void test_text_not_batched_into_single_send() {
  TEST("text is NOT a single batched send");
  reset();

  const char* tokens[] = {"First", ".", " Second", "."};
  for (auto t : tokens) onToken(t);
  onGroqDone();

  // Should be 2 sentence sends, not 1 big batch
  int sendCount = 0;
  for (auto& c : ttsLog) if (c.type == TTSCall::SEND_TEXT) sendCount++;
  assert(sendCount == 2);
  PASS();
}

void test_trailing_text_flushed() {
  TEST("trailing text without punctuation flushed");
  reset();

  // No sentence-ending punctuation
  onToken("Hello");
  onToken(" world");
  onGroqDone();

  // Remaining buffer sent in onGroqDone + flush
  assert(ttsLog.size() == 2);
  assert(ttsLog[0].type == TTSCall::SEND_TEXT);
  assert(ttsLog[0].text == "Hello world");
  assert(ttsLog[1].type == TTSCall::FLUSH);
  PASS();
}

void test_flush_sent_only_once() {
  TEST("flush sent exactly once after all text");
  reset();

  onToken("Hi");
  onToken("!");
  onGroqDone();

  int flushCount = 0;
  for (auto& c : ttsLog) if (c.type == TTSCall::FLUSH) flushCount++;
  assert(flushCount == 1);
  assert(ttsLog.back().type == TTSCall::FLUSH);
  PASS();
}

void test_no_tts_when_disconnected() {
  TEST("no TTS calls when EL disconnected");
  reset();
  elConnected = false;

  onToken("Hello");
  onToken(".");
  onGroqDone();

  assert(ttsLog.empty());
  PASS();
}

void test_response_still_accumulated() {
  TEST("currentResponse accumulated for history");
  reset();

  onToken("Hello");
  onToken(" world");
  onToken(".");

  assert(currentResponse == "Hello world.");
  PASS();
}

void test_question_mark_triggers_send() {
  TEST("question mark triggers sentence send");
  reset();

  onToken("How");
  onToken(" are");
  onToken(" you");
  onToken("?");

  assert(ttsLog.size() == 1);
  assert(ttsLog[0].text == "How are you?");
  PASS();
}

void test_multiple_sentences_streamed() {
  TEST("3 sentences streamed before Groq done");
  reset();

  // "A. B! C?"
  onToken("A");
  onToken(".");
  assert(ttsLog.size() == 1);

  onToken(" B");
  onToken("!");
  assert(ttsLog.size() == 2);

  onToken(" C");
  onToken("?");
  assert(ttsLog.size() == 3);

  onGroqDone();
  assert(ttsLog.size() == 4); // + flush
  PASS();
}

int main() {
  printf("=== Streaming TTS Tests ===\n");
  test_sentence_chunks_sent();
  test_text_not_batched_into_single_send();
  test_trailing_text_flushed();
  test_flush_sent_only_once();
  test_no_tts_when_disconnected();
  test_response_still_accumulated();
  test_question_mark_triggers_send();
  test_multiple_sentences_streamed();
  printf("\nPassed: %d, Failed: %d\n\n", tests_passed, tests_failed);
  return tests_failed > 0 ? 1 : 0;
}
