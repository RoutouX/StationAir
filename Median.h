#pragma once
#include <Arduino.h>

// Median on uint16_t (copy + insertion sort)
inline uint16_t median_u16(const uint16_t* src, uint16_t n) {
  if (n == 0) return 0;

  // n is small (<= a few hundreds). Allocate on stack carefully.
  // We'll sort in-place a temp buffer allocated by caller if needed.
  // Here we just do a simple selection on a local buffer, caller ensures n reasonable.
  // (Implemented as insertion sort on a local VLA is not allowed in C++ on Arduino)
  // => We'll expect caller to provide a temp buffer if needed in future.
  // For now, keep a fixed maximum in caller.

  // This header now expects caller to pass a PRE-COPIED buffer if they need.
  // To keep it simple, we implement a small sort on a local buffer with dynamic alloc is avoided.
  // => We'll provide a helper that sorts caller buffer (mutable).
  return 0; // not used directly
}

// Sort helper (insertion) on a mutable array
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
