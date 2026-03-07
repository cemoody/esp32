#pragma once

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class AudioBuffer {
public:
  AudioBuffer();
  ~AudioBuffer();

  bool init(size_t size);
  size_t write(const uint8_t* data, size_t len);
  size_t read(uint8_t* data, size_t len);
  size_t available() const;
  size_t freeSpace() const;
  void flush();

private:
  uint8_t* _buffer;
  size_t _size;
  volatile size_t _readPos;
  volatile size_t _writePos;
  volatile size_t _count;
  SemaphoreHandle_t _mutex;
};
