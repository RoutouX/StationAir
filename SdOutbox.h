#pragma once
#include <Arduino.h>
#include <SD.h>
#include "Config.h"
#include "BleLink.h"

class SdOutbox {
public:
  bool begin(int pinSd);
  bool ready() const { return isReady; }
  void tryReinit(int pinSd);

  void appendLine(const char* line);

  // Flush queued lines through BLE (reliable ACK), keeps not-sent lines.
  void flushIfAny(BleLink& ble);

private:
  bool isReady = false;

  void writeHeaderIfNeeded(File& f);
};
