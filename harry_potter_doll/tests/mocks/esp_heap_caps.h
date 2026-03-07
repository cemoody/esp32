#pragma once
#include <cstdlib>
#include <cstdint>

#define MALLOC_CAP_SPIRAM  (1 << 0)
#define MALLOC_CAP_8BIT    (1 << 1)

inline void* heap_caps_malloc(size_t size, uint32_t) {
  return malloc(size);
}

inline uint32_t esp_get_free_heap_size() { return 8 * 1024 * 1024; }
