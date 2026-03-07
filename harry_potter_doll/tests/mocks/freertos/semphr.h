#pragma once
#include "FreeRTOS.h"

typedef void* SemaphoreHandle_t;

static int _mock_sem = 1;

inline SemaphoreHandle_t xSemaphoreCreateMutex() { return (SemaphoreHandle_t)&_mock_sem; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return 1; }
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t) { return 1; }
inline void vSemaphoreDelete(SemaphoreHandle_t) {}
