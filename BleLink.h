#pragma once
#include <Arduino.h>
#include <ArduinoBLE.h>
#include "Config.h"

class BleLink {
public:
  bool begin();
  void poll();
  void onAckUpdate();

  bool isConnected() const;
  bool justConnected();
  bool justDisconnected();
  const char* centralAddress() const;

  void setEco2(uint16_t eco2);
  void setTime(const char* timeStr);

  bool sendRecordWithAck(uint32_t seq, const char* dateStr, const char* payloadLine);

private:
  BLEService envService{ENV_SERVICE_UUID};

  BLEUnsignedIntCharacteristic eco2Char{ECO2_CHAR_UUID, BLERead | BLENotify};
  BLEStringCharacteristic     timeChar{TIME_CHAR_UUID, BLERead | BLENotify, 20};

  BLEUnsignedIntCharacteristic seqChar{SEQ_CHAR_UUID, BLERead | BLENotify};
  BLEUnsignedIntCharacteristic ackChar{ACK_CHAR_UUID, BLEWrite};
  BLEStringCharacteristic      payloadChar{PAYLOAD_UUID, BLERead | BLENotify, 80};

  mutable String lastCentralAddr;
  volatile uint32_t lastAckSeq = 0;

  bool waitAck(uint32_t seq, unsigned long timeoutMs);
};
