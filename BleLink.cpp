#include "BleLink.h"

bool BleLink::begin() {
  if (!BLE.begin()) return false;

  BLE.setLocalName(BLE_LOCAL_NAME);
  BLE.setAdvertisedService(envService);

  envService.addCharacteristic(eco2Char);
  envService.addCharacteristic(timeChar);
  envService.addCharacteristic(seqChar);
  envService.addCharacteristic(ackChar);
  envService.addCharacteristic(payloadChar);

  BLE.addService(envService);

  eco2Char.writeValue(0);
  timeChar.writeValue("00/00 00:00:00");
  seqChar.writeValue(0);
  ackChar.writeValue(0);
  payloadChar.writeValue("");

  BLE.advertise();
  return true;
}

void BleLink::poll() {
  BLE.poll();

  BLEDevice central = BLE.central();
  if (central && central.connected()) {
    lastCentralAddr = central.address();
  }
}

void BleLink::onAckUpdate() {
  if (ackChar.written()) {
    uint32_t v = 0;
    ackChar.readValue(v);
    lastAckSeq = v;
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
  eco2Char.writeValue(eco2);
}

void BleLink::setTime(const char* timeStr) {
  timeChar.writeValue(timeStr);
}

bool BleLink::waitAck(uint32_t seq, unsigned long timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    BLE.poll();
    onAckUpdate();

    if (lastAckSeq == seq) return true;

    BLEDevice central = BLE.central();
    if (!central || !central.connected()) return false;

    delay(2);
  }
  return false;
}

bool BleLink::sendRecordWithAck(uint32_t seq, const char* dateStr, const char* payloadLine) {
  BLEDevice central = BLE.central();
  if (!central || !central.connected()) return false;

  setTime(dateStr);
  seqChar.writeValue(seq);
  payloadChar.writeValue(payloadLine);

  return waitAck(seq, ACK_TIMEOUT_MS);
}
