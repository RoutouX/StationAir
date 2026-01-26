#pragma once
#include <Arduino.h>
#include "RTClib.h"

inline void formatDateStr(char* out, size_t outSz, const DateTime& now) {
  snprintf(out, outSz, "%02d/%02d %02d:%02d:%02d",
           now.day(), now.month(), now.hour(), now.minute(), now.second());
}
