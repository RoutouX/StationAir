#include "SdOutbox.h"

static bool readFirstLine(File& f, String& out) {
  out = "";
  while (f.available()) {
    char c = (char)f.read();
    if (c == '\r') continue;
    if (c == '\n') break;
    out += c;
    if (out.length() > 200) break; // sécurité
  }
  return out.length() > 0;
}

bool SdOutbox::begin(int csPin) {
    _sdOk = SD.begin(csPin);
    if (_sdOk) {
        Serial.println(">> [SD] Initialisation REUSSIE");
    } else {
        Serial.println("!! [SD] Initialisation ECHOUEE");
    }
    return _sdOk;
}

void SdOutbox::tryReinit(int csPin) {
    if (_sdOk) return;
    if (SD.begin(csPin)) {
        Serial.println(">> [SD] Carte retrouvee !");
        _sdOk = true;
    }
}

void SdOutbox::appendLine(const char* line) {
    if (!_sdOk) return;

    bool exists = SD.exists(OUTBOX_FILE);
    File f = SD.open(OUTBOX_FILE, FILE_WRITE);
    
    if (f) {
        if (!exists || f.size() == 0) {
            Serial.println(">> [SD] Creation nouveau fichier + Entete");
            f.println("seq;unix;date;eco2_med;uptime_s");
        }
        f.println(line);
        f.flush(); 
        f.close();
        Serial.print(">> [SD] Enregistre : "); Serial.println(line);
    } else {
        Serial.println("!! [SD] Erreur d'ouverture FILE_WRITE");
        _sdOk = false;
    }
}

bool SdOutbox::parseSeq_(const char* line, uint32_t& outSeq) {
  if (!line) return false;
  const char* semi = strchr(line, ';');
  if (!semi) return false;
  char buf[16];
  size_t n = (size_t)(semi - line);
  if (n == 0 || n >= sizeof(buf)) return false;
  memcpy(buf, line, n);
  buf[n] = '\0';
  outSeq = (uint32_t)strtoul(buf, nullptr, 10);
  return outSeq != 0;
}

void SdOutbox::removeFirstLine_() {
  File in = SD.open(OUTBOX_FILE, FILE_READ);
  if (!in) return;

  // IMPORTANT : On utilise O_TRUNC pour effacer OUTBOX_NEW s'il existe deja
  SD.remove(OUTBOX_NEW); 
  File out = SD.open(OUTBOX_NEW, FILE_WRITE);
  
  if (!out) {
    in.close();
    return;
  }

  bool firstSkipped = false;
  while (in.available()) {
    String line;
    if (!readFirstLine(in, line)) break;
    if (!firstSkipped) {
      firstSkipped = true; 
      continue;
    }
    out.println(line);
  }
  in.close();
  out.close();

  // On ecrase l'ancien par le nouveau
  SD.remove(OUTBOX_FILE);
  File src = SD.open(OUTBOX_NEW, FILE_READ);
  File dst = SD.open(OUTBOX_FILE, FILE_WRITE);
  
  if (src && dst) {
    while (src.available()) {
      dst.write(src.read());
    }
  }
  if (src) src.close();
  if (dst) dst.close();
  SD.remove(OUTBOX_NEW);
}

void SdOutbox::flushIfAny(BleLink& ble) {
  if (!_sdOk || !ble.isConnected()) return;

  // On limite le nombre de lignes par flush pour ne pas bloquer le loop trop longtemps
  int limit = 10; 
  while (limit > 0) {
    File f = SD.open(OUTBOX_FILE, FILE_READ);
    if (!f) return;

    String line;
    bool hasLine = readFirstLine(f, line);
    f.close();

    // Si c'est l'entête, on la saute
    if (hasLine && line.startsWith("seq;")) {
        removeFirstLine_();
        continue;
    }

    if (!hasLine) return; 

    uint32_t seq = 0;
    if (!parseSeq_(line.c_str(), seq)) {
      removeFirstLine_();
      continue;
    }

    if (!ble.sendRecord(seq, line.c_str())) return;

    unsigned long start = millis();
    while (!ble.ackReceived() && !ble.ackTimedOut()) {
      ble.poll();
      if (millis() - start > (ACK_TIMEOUT_MS + 100)) break;
    }

    if (ble.ackReceived()) {
      ble.resetAck();
      removeFirstLine_();
      limit--;
    } else {
      ble.resetAck();
      return; 
    }
  }
}