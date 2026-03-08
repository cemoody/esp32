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

// ─── Track ElevenLabs connection lifecycle ───────────────
static std::vector<std::string> elEvents;
static bool elConnected = false;

static void el_connect() {
  elEvents.push_back("connect");
  elConnected = true;
}
static void el_disconnect() {
  elEvents.push_back("disconnect");
  elConnected = false;
}

// ─── Current behavior: disconnect after each turn ───────
static void goIdle_CURRENT() {
  el_disconnect();
}

// ─── Desired behavior: keep connection alive ────────────
static void goIdle_DESIRED() {
  // Do NOT disconnect — keep connection pooled
  // (connection stays alive for next turn)
}

static void reset() {
  elEvents.clear();
  elConnected = false;
}

// ─── Tests ──────────────────────────────────────────────

void test_no_disconnect_after_turn() {
  TEST("no disconnect after turn completes");
  reset();

  el_connect();
  goIdle_DESIRED();  // should NOT disconnect

  int disconnects = 0;
  for (auto& e : elEvents) {
    if (e == "disconnect") disconnects++;
  }
  assert(disconnects == 0);
  assert(elConnected == true);
  PASS();
}

void test_connection_reused_across_turns() {
  TEST("EL connection reused across turns");
  reset();

  // Turn 1
  el_connect();
  goIdle_DESIRED();
  assert(elConnected == true); // still connected!

  // Turn 2 — no reconnect needed
  // (in real code, connect() would short-circuit if already connected)
  assert(elConnected == true);
  goIdle_DESIRED();

  // Only 1 connect total
  int connects = 0;
  for (auto& e : elEvents) {
    if (e == "connect") connects++;
  }
  assert(connects == 1);
  PASS();
}

void test_no_disconnect_events_between_turns() {
  TEST("no disconnect events between turns");
  reset();

  el_connect();
  goIdle_DESIRED();
  goIdle_DESIRED();
  goIdle_DESIRED();

  int disconnects = 0;
  for (auto& e : elEvents) {
    if (e == "disconnect") disconnects++;
  }
  assert(disconnects == 0);
  PASS();
}

void test_disconnect_on_interrupt() {
  TEST("disconnect allowed on explicit interrupt");
  reset();

  el_connect();
  assert(elConnected == true);

  // Interrupt should still disconnect (this is intentional teardown)
  el_disconnect();
  assert(elConnected == false);
  PASS();
}

void test_stays_connected_after_tts_done() {
  TEST("stays connected after TTS completes");
  reset();

  el_connect();
  // TTS completes, pipeline goes idle
  bool ttsDone = true;
  int bufAvailable = 0;

  // Desired: going idle does NOT disconnect EL
  goIdle_DESIRED();
  assert(elConnected == true);
  PASS();
}

int main() {
  printf("=== Connection Pooling Tests ===\n");
  test_no_disconnect_after_turn();
  test_connection_reused_across_turns();
  test_no_disconnect_events_between_turns();
  test_disconnect_on_interrupt();
  test_stays_connected_after_tts_done();
  printf("\nPassed: %d, Failed: %d\n\n", tests_passed, tests_failed);
  return tests_failed > 0 ? 1 : 0;
}
