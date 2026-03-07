#pragma once
#include "esp_err.h"
#include "esp_log.h"

#define ESP_RETURN_ON_FALSE(cond, err, tag, fmt, ...) do { \
  if (!(cond)) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); return err; } \
} while(0)

#define ESP_RETURN_ON_ERROR(err_rc, tag, fmt, ...) do { \
  esp_err_t _rc = (err_rc); \
  if (_rc != ESP_OK) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); return _rc; } \
} while(0)

#define ESP_GOTO_ON_FALSE(cond, err, label, tag, fmt, ...) do { \
  if (!(cond)) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); ret = err; goto label; } \
} while(0)

#define ESP_GOTO_ON_ERROR(err_rc, label, tag, fmt, ...) do { \
  esp_err_t _rc = (err_rc); \
  if (_rc != ESP_OK) { ESP_LOGE(tag, fmt, ##__VA_ARGS__); ret = _rc; goto label; } \
} while(0)
