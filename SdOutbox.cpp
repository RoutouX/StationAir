#include "SdOutbox.h"

bool SdOutbox::begin(int pinSd) {
  isReady = SD.begin(pinSd);
  return isReady;
}

void SdOutbox::tryReinit(int pinSd) {
  if (isReady) return;
  if (SD.begin(pinSd)) {
    Serial.println("OK SD retrouvee");
    isReady = true;
  }
}

void SdOutbox::writeHeaderIfNeeded(File& f) {
  if (f.size() == 0) {
    f.println("seq;unix;date;eco2_med;uptime_s");
  }
}

void SdOutbox::appendLine(const char* line) {
  if (!isReady) {
    Serial.println("!! SD not ready, skip append");
    return;
  }

  Serial.println(">> [SD] append OUTBOX");
  File f = SD.open(OUTBOX_FILE, FILE_WRITE);
  if (!f) {
    Serial.println("ERR SD: OUTBOX open failed");
    isReady = false;
    return;
  }

  writeHeaderIfNeeded(f);
  f.println(line);
  f.close();
}

void SdOutbox::flushIfAny(BleLink& ble) {
  // Vérifications de base
  if (!isReady || !ble.isConnected()) return;
  if (!SD.exists(OUTBOX_FILE)) return;

  File in = SD.open(OUTBOX_FILE, FILE_READ);
  File newf = SD.open(OUTBOX_NEW, FILE_WRITE); // Fichier temporaire
  
  if (!in || !newf) return;

  Serial.println(">> Flush SD -> BLE");

  bool firstLine = true;

  while (in.available()) {
    String line = in.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // Gestion de l'entête (on le recopie juste)
    if (firstLine) {
      firstLine = false;
      if (line.startsWith("seq;")) {
        newf.println(line);
        continue;
      } else {
        // Si l'entête manquait, on l'ajoute
        newf.println("seq;unix;date;eco2_med;uptime_s");
      }
    }

    // === CORRECTION ICI ===
    // On n'a plus besoin de découper la ligne (parsing) car sendRecord prend tout.
    // On envoie la ligne complète directement.
    
    // sendRecord est bloquant et utilise 'Indicate' pour garantir la réception
    bool ok = ble.sendRecord(line.c_str()); 

    if (!ok) {
      // Si l'envoi échoue (pas d'ACK ou déconnexion), 
      // on sauvegarde la ligne dans le nouveau fichier pour réessayer plus tard.
      newf.println(line);
      
      // Optionnel : On peut arrêter le flush ici pour ne pas bloquer 
      // si la connexion est perdue au milieu
      break; 
    }
    // Si ok, on ne fait rien (la ligne n'est pas copiée dans newf, donc elle est "supprimée" de la liste d'attente)
  }

  in.close();
  newf.close();

  // Remplacement de l'ancien fichier par le nouveau (qui contient les restes non envoyés)
  SD.remove(OUTBOX_FILE);
  
  // Copie physique (SD vers SD) pour renommer proprement
  File src = SD.open(OUTBOX_NEW, FILE_READ);
  File dst = SD.open(OUTBOX_FILE, FILE_WRITE);
  while (src.available()) dst.write(src.read());
  src.close();
  dst.close();
  
  SD.remove(OUTBOX_NEW);
}