#pragma once
#include <cstdint>
#include <cstddef>

// Portable base64 decode implementation for host testing
// Returns 0 on success, non-zero on error
// If dst is NULL, only calculates output length in *olen
inline int mbedtls_base64_decode(
    unsigned char *dst, size_t dlen, size_t *olen,
    const unsigned char *src, size_t slen)
{
  static const unsigned char decode_table[256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255, 62,255,255,255, 63,
     52, 53, 54, 55, 56, 57, 58, 59, 60, 61,255,255,255,  0,255,255,
    255,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,255,255,255,255,255,
    255, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
     41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
  };

  // Calculate output length
  size_t n = 0;
  for (size_t i = 0; i < slen; i++) {
    if (src[i] == '=') break;
    if (decode_table[src[i]] == 255) continue;
    n++;
  }
  size_t out_len = (n * 3) / 4;
  *olen = out_len;

  if (dst == nullptr) return 0; // Just calculating length
  if (dlen < out_len) return -1; // Buffer too small

  size_t j = 0;
  uint32_t accum = 0;
  int bits = 0;
  for (size_t i = 0; i < slen; i++) {
    if (src[i] == '=') break;
    unsigned char val = decode_table[src[i]];
    if (val == 255) continue;
    accum = (accum << 6) | val;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      dst[j++] = (unsigned char)((accum >> bits) & 0xFF);
    }
  }

  *olen = j;
  return 0;
}
