#include "audio_buffer.h"
#include <string.h>
#include <stdlib.h>
#include "esp_heap_caps.h"

AudioBuffer::AudioBuffer()
  : _buffer(nullptr), _size(0), _readPos(0), _writePos(0), _count(0), _mutex(nullptr) {}

AudioBuffer::~AudioBuffer() {
  if (_buffer) free(_buffer);
  if (_mutex) vSemaphoreDelete(_mutex);
}

bool AudioBuffer::init(size_t size) {
  _buffer = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!_buffer) {
    _buffer = (uint8_t*)malloc(size);
  }
  if (!_buffer) return false;

  _size = size;
  _readPos = 0;
  _writePos = 0;
  _count = 0;
  _mutex = xSemaphoreCreateMutex();
  return _mutex != nullptr;
}

size_t AudioBuffer::write(const uint8_t* data, size_t len) {
  if (!_buffer || !data || len == 0) return 0;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  size_t space = _size - _count;
  size_t toWrite = (len < space) ? len : space;

  for (size_t i = 0; i < toWrite; i++) {
    _buffer[_writePos] = data[i];
    _writePos = (_writePos + 1) % _size;
  }
  _count += toWrite;
  xSemaphoreGive(_mutex);

  return toWrite;
}

size_t AudioBuffer::read(uint8_t* data, size_t len) {
  if (!_buffer || !data || len == 0) return 0;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  size_t toRead = (len < _count) ? len : _count;

  for (size_t i = 0; i < toRead; i++) {
    data[i] = _buffer[_readPos];
    _readPos = (_readPos + 1) % _size;
  }
  _count -= toRead;
  xSemaphoreGive(_mutex);

  return toRead;
}

size_t AudioBuffer::available() const {
  return _count;
}

size_t AudioBuffer::freeSpace() const {
  return _size - _count;
}

void AudioBuffer::flush() {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _readPos = 0;
  _writePos = 0;
  _count = 0;
  xSemaphoreGive(_mutex);
}
