#include "audio_buffer.h"
#include <cassert>
#include <cstdio>
#include <cstring>

static int tests_passed = 0;
#define TEST(name) printf("  %-40s", name);
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)

void test_init() {
  TEST("init allocates buffer");
  AudioBuffer buf;
  assert(buf.init(1024));
  assert(buf.available() == 0);
  assert(buf.freeSpace() == 1024);
  PASS();
}

void test_write_read_roundtrip() {
  TEST("write/read roundtrip");
  AudioBuffer buf;
  buf.init(256);

  uint8_t input[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  assert(buf.write(input, 10) == 10);
  assert(buf.available() == 10);
  assert(buf.freeSpace() == 246);

  uint8_t output[10] = {};
  assert(buf.read(output, 10) == 10);
  assert(memcmp(input, output, 10) == 0);
  assert(buf.available() == 0);
  PASS();
}

void test_circular_wraparound() {
  TEST("circular wraparound");
  AudioBuffer buf;
  buf.init(8);

  uint8_t data[6] = {1, 2, 3, 4, 5, 6};
  assert(buf.write(data, 6) == 6);

  uint8_t out[4];
  assert(buf.read(out, 4) == 4); // read 4, leaves 2 at positions 4,5
  assert(out[0] == 1 && out[3] == 4);

  // Now write 6 more — wraps around past end
  uint8_t data2[6] = {10, 11, 12, 13, 14, 15};
  assert(buf.write(data2, 6) == 6);
  assert(buf.available() == 8); // 2 remaining + 6 new

  uint8_t out2[8];
  assert(buf.read(out2, 8) == 8);
  assert(out2[0] == 5 && out2[1] == 6); // the 2 remaining
  assert(out2[2] == 10 && out2[7] == 15); // the 6 new
  PASS();
}

void test_buffer_full() {
  TEST("buffer full partial write");
  AudioBuffer buf;
  buf.init(4);

  uint8_t data[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  assert(buf.write(data, 10) == 4); // only 4 fit
  assert(buf.available() == 4);
  assert(buf.freeSpace() == 0);
  PASS();
}

void test_buffer_empty_read() {
  TEST("empty buffer read returns 0");
  AudioBuffer buf;
  buf.init(64);

  uint8_t out[10];
  assert(buf.read(out, 10) == 0);
  PASS();
}

void test_flush() {
  TEST("flush clears data");
  AudioBuffer buf;
  buf.init(64);

  uint8_t data[20];
  memset(data, 0xAA, 20);
  buf.write(data, 20);
  assert(buf.available() == 20);

  buf.flush();
  assert(buf.available() == 0);
  assert(buf.freeSpace() == 64);
  PASS();
}

void test_null_safety() {
  TEST("null/zero args safety");
  AudioBuffer buf;
  buf.init(64);

  assert(buf.write(nullptr, 10) == 0);
  uint8_t d[1] = {1};
  assert(buf.write(d, 0) == 0);
  assert(buf.read(nullptr, 10) == 0);
  PASS();
}

int main() {
  printf("=== AudioBuffer Tests ===\n");
  test_init();
  test_write_read_roundtrip();
  test_circular_wraparound();
  test_buffer_full();
  test_buffer_empty_read();
  test_flush();
  test_null_safety();
  printf("All %d tests passed.\n\n", tests_passed);
  return 0;
}
