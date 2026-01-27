#pragma once
#include <Arduino.h>
#include <SD.h>
#include "Config.h"
#include "BleLink.h"

class SdOutbox {
public:
  bool begin(int pinSd);
  void tryReinit(int pinSd);

  void appendLine(const char* line);
  void flushIfAny(BleLink& ble);

private:
  bool isReady = false;
  void writeHeaderIfNeeded(File& f);
};
