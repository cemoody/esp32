#pragma once
#include <cstdint>

#define portMAX_DELAY 0xFFFFFFFF
#define pdMS_TO_TICKS(ms) (ms)

typedef uint32_t TickType_t;
typedef int BaseType_t;

inline void vTaskDelay(TickType_t) {}
