#include "Arduino.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static int tests_passed = 0;
#define TEST(name) printf("  %-40s", name);
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)

// Replicate ConversationTurn and history management from the .ino
struct ConversationTurn {
  String role;
  String content;
};

#define MAX_CONVERSATION_TURNS 10

static ConversationTurn conversationHistory[MAX_CONVERSATION_TURNS];
static size_t historyCount = 0;

static void resetHistory() {
  historyCount = 0;
  for (int i = 0; i < MAX_CONVERSATION_TURNS; i++) {
    conversationHistory[i].role = "";
    conversationHistory[i].content = "";
  }
}

// Extracted from harry_potter_doll.ino
static void addToHistory(const char* role, const char* content) {
  if (historyCount >= MAX_CONVERSATION_TURNS) {
    for (size_t i = 0; i < MAX_CONVERSATION_TURNS - 1; i++) {
      conversationHistory[i] = conversationHistory[i + 1];
    }
    historyCount = MAX_CONVERSATION_TURNS - 1;
  }
  conversationHistory[historyCount].role = role;
  conversationHistory[historyCount].content = content;
  historyCount++;
}

void test_add_single() {
  TEST("add single turn");
  resetHistory();
  addToHistory("user", "Hello Harry");
  assert(historyCount == 1);
  assert(conversationHistory[0].role == "user");
  assert(conversationHistory[0].content == "Hello Harry");
  PASS();
}

void test_add_conversation() {
  TEST("add user + assistant pair");
  resetHistory();
  addToHistory("user", "Hello");
  addToHistory("assistant", "Hi there!");
  assert(historyCount == 2);
  assert(conversationHistory[0].role == "user");
  assert(conversationHistory[1].role == "assistant");
  PASS();
}

void test_rotation_at_limit() {
  TEST("oldest dropped at MAX_CONVERSATION_TURNS");
  resetHistory();

  // Fill to capacity
  for (int i = 0; i < MAX_CONVERSATION_TURNS; i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "message %d", i);
    addToHistory(i % 2 == 0 ? "user" : "assistant", buf);
  }
  assert(historyCount == MAX_CONVERSATION_TURNS);

  // Add one more — should drop oldest
  addToHistory("user", "overflow message");
  assert(historyCount == MAX_CONVERSATION_TURNS);

  // First entry should now be "message 1" (message 0 was dropped)
  assert(conversationHistory[0].content == "message 1");

  // Last entry should be the overflow
  assert(conversationHistory[MAX_CONVERSATION_TURNS - 1].content == "overflow message");
  PASS();
}

void test_multiple_rotations() {
  TEST("multiple overflows rotate correctly");
  resetHistory();

  // Add 15 messages (5 more than limit)
  for (int i = 0; i < 15; i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "msg %d", i);
    addToHistory("user", buf);
  }
  assert(historyCount == MAX_CONVERSATION_TURNS);

  // Should have messages 5-14
  assert(conversationHistory[0].content == "msg 5");
  assert(conversationHistory[9].content == "msg 14");
  PASS();
}

void test_history_preserves_roles() {
  TEST("roles preserved through rotation");
  resetHistory();

  for (int i = 0; i < 12; i++) {
    addToHistory(i % 2 == 0 ? "user" : "assistant", "x");
  }

  // After 12 additions, first entry was originally index 2 (user)
  assert(conversationHistory[0].role == "user");
  assert(conversationHistory[1].role == "assistant");
  PASS();
}

int main() {
  printf("=== Conversation History Tests ===\n");
  test_add_single();
  test_add_conversation();
  test_rotation_at_limit();
  test_multiple_rotations();
  test_history_preserves_roles();
  printf("All %d tests passed.\n\n", tests_passed);
  return 0;
}
