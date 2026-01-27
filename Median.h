#pragma once
#include <Arduino.h>

inline void sort_u16_inplace(uint16_t* a, uint16_t n) {
  for (uint16_t i = 1; i < n; i++) {
    uint16_t key = a[i];
    int32_t j = (int32_t)i - 1;
    while (j >= 0 && a[j] > key) {
      a[j + 1] = a[j];
      j--;
    }
    a[j + 1] = key;
  }
}

inline uint16_t median_u16_from_sorted(const uint16_t* a, uint16_t n) {
  if (n == 0) return 0;
  if (n & 1) return a[n / 2];
  uint32_t x = a[(n / 2) - 1];
  uint32_t y = a[n / 2];
  return (uint16_t)((x + y) / 2);
}
