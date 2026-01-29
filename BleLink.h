#pragma once
#include <Arduino.h>
#include <ArduinoBLE.h>
#include "Config.h"

class BleLink {
public:
  bool begin();
  void poll();

  // État de la connexion
  bool isConnected() const;
  bool justConnected();
  bool justDisconnected();
  const char* centralAddress() const;

  // Mise à jour des valeurs "Live"
  void setEco2(uint16_t eco2);
  void setTime(const char* timeStr);

  // Envoi de l'historique (BLOQUANT et SÉCURISÉ par BLEIndicate)
  // Retourne true si le téléphone a confirmé la réception
  bool sendRecord(const char* payloadLine);

private:
  BLEService envService{ENV_SERVICE_UUID};

  // Indicate garantit la réception pour le CO2 et l'heure
  BLEUnsignedIntCharacteristic eco2Char{ECO2_CHAR_UUID, BLERead | BLEIndicate};
  BLEStringCharacteristic      timeChar{TIME_CHAR_UUID, BLERead | BLEIndicate, 20};

  // La caractéristique pour envoyer la ligne CSV complète
  // Taille max 80 (Attention au MTU, voir note plus bas)
  BLEStringCharacteristic      payloadChar{PAYLOAD_UUID, BLERead | BLEIndicate, 80};

  mutable String lastCentralAddr;
};