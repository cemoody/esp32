#include "Arduino.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

static int tests_passed = 0;
#define TEST(name) printf("  %-40s", name);
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)

// Mirrors the updated groq.cpp processSSEStream token extraction with escape handling
static std::vector<std::string> extractTokensFromSSE(const std::string& sseData) {
  std::vector<std::string> tokens;
  String buffer;

  for (char c : sseData) {
    buffer += c;
    if (c == '\n') {
      buffer.trim();

      if (buffer.startsWith("data: ")) {
        String data = buffer.substring(6);

        if (data == "[DONE]") break;

        int idx = data.indexOf("\"content\":\"");
        if (idx >= 0) {
          idx += 11;
          // Find closing quote, skipping escaped ones
          int end = idx;
          const char* s = data.c_str();
          while (s[end] && !(s[end] == '"' && (end == idx || s[end - 1] != '\\'))) {
            end++;
          }
          if (end > idx && s[end] == '"') {
            String content = data.substring(idx, end);
            // Unescape JSON sequences
            std::string unescaped;
            const char* cp = content.c_str();
            for (size_t i = 0; i < content.length(); i++) {
              if (cp[i] == '\\' && i + 1 < content.length()) {
                switch (cp[i + 1]) {
                  case '"': unescaped += '"'; i++; break;
                  case '\\': unescaped += '\\'; i++; break;
                  case 'n': unescaped += '\n'; i++; break;
                  case 't': unescaped += '\t'; i++; break;
                  default: unescaped += cp[i]; break;
                }
              } else {
                unescaped += cp[i];
              }
            }
            if (!unescaped.empty()) {
              tokens.push_back(unescaped);
            }
          }
        }
      }
      buffer = String("");
    }
  }
  return tokens;
}

void test_basic_token_extraction() {
  TEST("extract tokens from SSE lines");
  std::string sse =
    "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\" world\"}}]}\n"
    "\n"
    "data: [DONE]\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 2);
  assert(tokens[0] == "Hello");
  assert(tokens[1] == " world");
  PASS();
}

void test_chunked_encoding_ignored() {
  TEST("chunk size lines are ignored");
  std::string sse =
    "164\r\n"
    "data: {\"choices\":[{\"delta\":{\"role\":\"assistant\",\"content\":\"\"}}]}\n"
    "\r\n"
    "\r\n"
    "110\r\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\"I\"}}]}\n"
    "\r\n"
    "\r\n"
    "113\r\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\" think\"}}]}\n"
    "\n"
    "data: [DONE]\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 2);
  assert(tokens[0] == "I");
  assert(tokens[1] == " think");
  PASS();
}

void test_empty_content_skipped() {
  TEST("empty content delta skipped");
  std::string sse =
    "data: {\"choices\":[{\"delta\":{\"role\":\"assistant\",\"content\":\"\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\"Hi\"}}]}\n"
    "\n"
    "data: [DONE]\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 1);
  assert(tokens[0] == "Hi");
  PASS();
}

void test_done_terminates() {
  TEST("[DONE] stops parsing");
  std::string sse =
    "data: {\"choices\":[{\"delta\":{\"content\":\"before\"}}]}\n"
    "\n"
    "data: [DONE]\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\"after\"}}]}\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 1);
  assert(tokens[0] == "before");
  PASS();
}

void test_no_content_key() {
  TEST("missing content key handled");
  std::string sse =
    "data: {\"choices\":[{\"delta\":{\"role\":\"assistant\"}}]}\n"
    "\n"
    "data: [DONE]\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 0);
  PASS();
}

void test_multitoken_real_response() {
  TEST("real-world multi-token response");
  std::string sse =
    "data: {\"choices\":[{\"delta\":{\"content\":\"Bl\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\"ime\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\"y\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\",\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\" I\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\" love\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\" Hogwarts\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\"!\"}}]}\n"
    "\n"
    "data: [DONE]\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 8);

  std::string full;
  for (auto& t : tokens) full += t;
  assert(full == "Blimey, I love Hogwarts!");
  PASS();
}

void test_escaped_quotes_in_content() {
  TEST("escaped quotes in content");
  // JSON: "content":"He said \"hello\""
  std::string sse =
    "data: {\"choices\":[{\"delta\":{\"content\":\"He said \\\"hello\\\"\"}}]}\n"
    "\n"
    "data: [DONE]\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 1);
  assert(tokens[0] == "He said \"hello\"");
  PASS();
}

void test_escaped_backslash() {
  TEST("escaped backslash in content");
  // JSON: "content":"path\\to\\file"
  std::string sse =
    "data: {\"choices\":[{\"delta\":{\"content\":\"path\\\\to\\\\file\"}}]}\n"
    "\n"
    "data: [DONE]\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 1);
  assert(tokens[0] == "path\\to\\file");
  PASS();
}

void test_escaped_newline() {
  TEST("escaped newline in content");
  // JSON: "content":"line1\nline2"
  std::string sse =
    "data: {\"choices\":[{\"delta\":{\"content\":\"line1\\nline2\"}}]}\n"
    "\n"
    "data: [DONE]\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 1);
  assert(tokens[0] == "line1\nline2");
  PASS();
}

void test_apostrophe_s() {
  TEST("apostrophe-s (Groq common pattern)");
  // Groq sends 's as a separate token: "content":"'s"
  std::string sse =
    "data: {\"choices\":[{\"delta\":{\"content\":\"it\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\"'s\"}}]}\n"
    "\n"
    "data: {\"choices\":[{\"delta\":{\"content\":\" great\"}}]}\n"
    "\n"
    "data: [DONE]\n";

  auto tokens = extractTokensFromSSE(sse);
  assert(tokens.size() == 3);
  std::string full;
  for (auto& t : tokens) full += t;
  assert(full == "it's great");
  PASS();
}

int main() {
  printf("=== Groq SSE Parser Tests ===\n");
  test_basic_token_extraction();
  test_chunked_encoding_ignored();
  test_empty_content_skipped();
  test_done_terminates();
  test_no_content_key();
  test_multitoken_real_response();
  test_escaped_quotes_in_content();
  test_escaped_backslash();
  test_escaped_newline();
  test_apostrophe_s();
  printf("All %d tests passed.\n\n", tests_passed);
  return 0;
}
