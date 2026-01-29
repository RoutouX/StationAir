#include "BleLink.h"

bool BleLink::begin() {
  if (!BLE.begin()) return false;

  BLE.setLocalName(BLE_LOCAL_NAME);
  BLE.setAdvertisedService(envService);

  envService.addCharacteristic(eco2Char);
  envService.addCharacteristic(timeChar);
  envService.addCharacteristic(payloadChar);
  envService.addCharacteristic(ackChar); // ✅ ajout ACK

  BLE.addService(envService);

  eco2Char.writeValue(0);
  timeChar.writeValue("00/00 00:00:00");
  payloadChar.writeValue("");
  // ackChar n'a pas besoin de valeur initiale

  BLE.advertise();
  return true;
}

void BleLink::poll() {
  BLE.poll();

  BLEDevice central = BLE.central();
  if (central && central.connected()) {
    lastCentralAddr = central.address();
  }

  // ✅ Traitement ACK (central écrit un uint32)
  if (ackChar.written()) {
    uint32_t got = 0;
    ackChar.readValue(got);

    if (_waitingAckSeq != 0 && got == _waitingAckSeq) {
      _ackOk = true;
      _waitingAckSeq = 0;
    }
  }
}

bool BleLink::isConnected() const {
  BLEDevice central = BLE.central();
  return (central && central.connected());
}

bool BleLink::justConnected() {
  static bool last = false;
  bool now = isConnected();
  bool jc = (now && !last);
  last = now;
  return jc;
}

bool BleLink::justDisconnected() {
  static bool last = false;
  bool now = isConnected();
  bool jd = (!now && last);
  last = now;
  return jd;
}

const char* BleLink::centralAddress() const {
  return lastCentralAddr.c_str();
}

void BleLink::setEco2(uint16_t eco2) {
  if (isConnected()) {
    eco2Char.writeValue(eco2);
  }
}

void BleLink::setTime(const char* timeStr) {
  if (isConnected()) {
    timeChar.writeValue(timeStr);
  }
}

bool BleLink::sendRecord(uint32_t seq, const char* payloadLine) {
  BLEDevice central = BLE.central();
  if (!central || !central.connected()) return false;

  _ackOk = false;
  _waitingAckSeq = seq;
  _ackStartMs = millis();

  // ✅ NOTIFY => non bloquant
  return payloadChar.writeValue(payloadLine);
}

bool BleLink::ackTimedOut() const {
  if (_waitingAckSeq == 0) return false; // plus en attente
  return (millis() - _ackStartMs) > ACK_TIMEOUT_MS;
}

void BleLink::resetAck() {
  _ackOk = false;
  _waitingAckSeq = 0;
  _ackStartMs = 0;
}
