#include "BleLink.h"

bool BleLink::begin() {
  if (!BLE.begin()) return false;

  BLE.setLocalName(BLE_LOCAL_NAME);
  BLE.setAdvertisedService(envService);

  // Ajout des caractéristiques
  envService.addCharacteristic(eco2Char);
  envService.addCharacteristic(timeChar);
  envService.addCharacteristic(payloadChar);

  BLE.addService(envService);

  // Valeurs initiales
  eco2Char.writeValue(0);
  timeChar.writeValue("00/00 00:00:00");
  payloadChar.writeValue("");

  BLE.advertise();
  return true;
}

void BleLink::poll() {
  BLE.poll();
  // Met à jour l'adresse si connecté
  BLEDevice central = BLE.central();
  if (central && central.connected()) {
    lastCentralAddr = central.address();
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
  // Avec Indicate, writeValue attend l'ACK du téléphone
  if (isConnected()) {
    eco2Char.writeValue(eco2); 
  }
}

void BleLink::setTime(const char* timeStr) {
  if (isConnected()) {
    timeChar.writeValue(timeStr);
  }
}

bool BleLink::sendRecord(const char* payloadLine) {
  BLEDevice central = BLE.central();
  
  // 1. Vérification basique
  if (!central || !central.connected()) return false;

  // 2. Envoi avec INDICATION
  // writeValue() va envoyer la donnée et attendre l'ACK du téléphone.
  // Si le téléphone ne répond pas, writeValue renvoie false (ou 0).
  bool success = payloadChar.writeValue(payloadLine);

  return success;
}