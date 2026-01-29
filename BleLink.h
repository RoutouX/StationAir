#pragma once
#include <Arduino.h>
#include <ArduinoBLE.h>
#include "Config.h"

class BleLink {
public:
  bool begin();
  void poll();

  bool isConnected() const;
  bool justConnected();
  bool justDisconnected();
  const char* centralAddress() const;

  void setEco2(uint16_t eco2);
  void setTime(const char* timeStr);

  // ✅ Envoi non bloquant (NOTIFY) + attente ACK applicatif via poll()
  // Retourne true si l'envoi notify a été "déclenché" (pas l'ACK)
  bool sendRecord(uint32_t seq, const char* payloadLine);

  // ✅ Etat ACK (applicatif)
  bool ackReceived() const { return _ackOk; }
  bool ackTimedOut() const;
  void resetAck();

private:
  BLEService envService{ENV_SERVICE_UUID};

  // ✅ NOTIFY => non bloquant
  BLEUnsignedIntCharacteristic eco2Char{ECO2_CHAR_UUID, BLERead | BLENotify};
  BLEStringCharacteristic      timeChar{TIME_CHAR_UUID, BLERead | BLENotify, 20};

  // ✅ NOTIFY pour les lignes
  BLEStringCharacteristic      payloadChar{PAYLOAD_UUID, BLERead | BLENotify, 80};

  // ✅ Le central écrit le seq acké
  BLEUnsignedIntCharacteristic ackChar{ACK_CHAR_UUID, BLEWriteWithoutResponse};

  mutable String lastCentralAddr;

  // ACK state
  uint32_t _waitingAckSeq = 0;
  unsigned long _ackStartMs = 0;
  bool _ackOk = false;
};