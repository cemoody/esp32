#include "Arduino.h"
#include "mbedtls/base64.h"
#include "esp_heap_caps.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int tests_passed = 0;
#define TEST(name) printf("  %-40s", name);
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)

// Extracted audio parsing logic from elevenlabs.cpp handleMessage
struct AudioResult {
  std::vector<uint8_t> pcm;
  bool isFinal;
};

static AudioResult parseELMessage(const char* data, size_t len) {
  AudioResult result;
  result.isFinal = false;

  if (strstr(data, "\"isFinal\":true")) {
    result.isFinal = true;
  }

  const char* audioStart = strstr(data, "\"audio\":\"");
  if (!audioStart) return result;
  audioStart += 9;

  const char* audioEnd = strchr(audioStart, '"');
  if (!audioEnd || audioEnd <= audioStart) return result;

  size_t b64Len = audioEnd - audioStart;

  // Check for null
  if (b64Len == 4 && strncmp(audioStart, "null", 4) == 0) return result;

  size_t outLen = 0;
  mbedtls_base64_decode(nullptr, 0, &outLen, (const uint8_t*)audioStart, b64Len);

  if (outLen > 0) {
    result.pcm.resize(outLen);
    size_t actualLen = 0;
    int ret = mbedtls_base64_decode(result.pcm.data(), outLen, &actualLen,
                                     (const uint8_t*)audioStart, b64Len);
    if (ret == 0) {
      result.pcm.resize(actualLen);
    } else {
      result.pcm.clear();
    }
  }

  return result;
}

void test_audio_extraction() {
  TEST("extract base64 audio from message");
  // "AQACAAMABAAFAA==" decodes to bytes: 1,0,2,0,3,0,4,0,5,0
  std::string msg = R"({"audio":"AQACAAMABAAFAA==","isFinal":false})";
  auto result = parseELMessage(msg.c_str(), msg.size());

  assert(!result.isFinal);
  assert(result.pcm.size() == 10);
  assert(result.pcm[0] == 1);
  assert(result.pcm[2] == 2);
  assert(result.pcm[4] == 3);
  assert(result.pcm[6] == 4);
  assert(result.pcm[8] == 5);
  PASS();
}

void test_null_audio() {
  TEST("null audio returns empty");
  std::string msg = R"({"audio":null,"isFinal":false})";
  auto result = parseELMessage(msg.c_str(), msg.size());

  assert(!result.isFinal);
  assert(result.pcm.empty());
  PASS();
}

void test_is_final_detection() {
  TEST("isFinal:true detected");
  std::string msg = R"({"audio":null,"isFinal":true,"normalizedAlignment":null})";
  auto result = parseELMessage(msg.c_str(), msg.size());

  assert(result.isFinal);
  assert(result.pcm.empty());
  PASS();
}

void test_is_final_false() {
  TEST("isFinal:false not flagged");
  std::string msg = R"({"audio":"AQAB","isFinal":false})";
  auto result = parseELMessage(msg.c_str(), msg.size());

  assert(!result.isFinal);
  assert(!result.pcm.empty());
  PASS();
}

void test_large_audio_chunk() {
  TEST("large base64 chunk (simulated 10KB)");
  // Generate 10KB of base64 data (all 'A's = zero bytes)
  std::string b64(13332, 'A'); // ~10KB decoded
  b64 += "==";
  std::string msg = "{\"audio\":\"" + b64 + "\",\"isFinal\":false}";

  auto result = parseELMessage(msg.c_str(), msg.size());
  assert(!result.isFinal);
  assert(result.pcm.size() > 9000);
  PASS();
}

void test_no_audio_key() {
  TEST("message without audio key");
  std::string msg = R"({"type":"metadata","request_id":"abc123"})";
  auto result = parseELMessage(msg.c_str(), msg.size());

  assert(!result.isFinal);
  assert(result.pcm.empty());
  PASS();
}

void test_base64_decode_correctness() {
  TEST("base64 decode known value");
  // "SGVsbG8=" = "Hello"
  std::string msg = R"({"audio":"SGVsbG8="})";
  auto result = parseELMessage(msg.c_str(), msg.size());

  assert(result.pcm.size() == 5);
  assert(memcmp(result.pcm.data(), "Hello", 5) == 0);
  PASS();
}

int main() {
  printf("=== ElevenLabs Audio Parser Tests ===\n");
  test_audio_extraction();
  test_null_audio();
  test_is_final_detection();
  test_is_final_false();
  test_large_audio_chunk();
  test_no_audio_key();
  test_base64_decode_correctness();
  printf("All %d tests passed.\n\n", tests_passed);
  return 0;
}
