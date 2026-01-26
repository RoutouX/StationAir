#pragma once
#include <Arduino.h>
#include "Config.h"

inline uint16_t median_u16(const uint16_t* src, uint8_t n) {
  if (n == 0) return 0;

  uint16_t tmp[MAX_SAMPLES_PER_MIN];
  for (uint8_t i = 0; i < n; i++) tmp[i] = src[i];

  // insertion sort (n <= 60)
  for (uint8_t i = 1; i < n; i++) {
    uint16_t key = tmp[i];
    int j = i - 1;
    while (j >= 0 && tmp[j] > key) {
      tmp[j + 1] = tmp[j];
      j--;
    }
    tmp[j + 1] = key;
  }

  if (n & 1) return tmp[n / 2];
  uint32_t a = tmp[(n / 2) - 1];
  uint32_t b = tmp[n / 2];
  return (uint16_t)((a + b) / 2);
}
