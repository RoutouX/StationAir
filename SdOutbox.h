#pragma once
#include <Arduino.h>
#include <SD.h>
#include "Config.h"
#include "BleLink.h"

class SdOutbox {
public:
  bool begin(int csPin);
  void tryReinit(int csPin);

  void appendLine(const char* line);
  void flushIfAny(BleLink& ble);

private:
  bool _sdOk = false;

  // Parse "seq;...." => seq (uint32)
  static bool parseSeq_(const char* line, uint32_t& outSeq);

  // Copie OUTBOX_FILE -> OUTBOX_NEW en supprimant la 1ère ligne si ack OK
  void removeFirstLine_();
};


