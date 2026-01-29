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
  return _sdOk;
}

void SdOutbox::tryReinit(int csPin) {
  if (_sdOk) return;
  _sdOk = SD.begin(csPin);
}

void SdOutbox::appendLine(const char* line) {
  if (!_sdOk || !line || !line[0]) return;

  File f = SD.open(OUTBOX_FILE, FILE_WRITE);
  if (!f) return;

  f.println(line);
  f.close();
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
  // 1. Ouvrir les fichiers pour préparer la version sans la première ligne
  File in = SD.open(OUTBOX_FILE, FILE_READ);
  if (!in) return;

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
      firstSkipped = true; // On ignore la première ligne ici
      continue;
    }
    out.println(line);
  }

  in.close();
  out.close();

  // === REMPLACEMENT DE SD.rename() ===
  // On supprime l'ancien fichier
  SD.remove(OUTBOX_FILE);

  // On recopie le contenu de OUTBOX_NEW vers OUTBOX_FILE
  File src = SD.open(OUTBOX_NEW, FILE_READ);
  File dst = SD.open(OUTBOX_FILE, FILE_WRITE);
  
  if (src && dst) {
    while (src.available()) {
      dst.write(src.read());
    }
  }

  if (src) src.close();
  if (dst) dst.close();

  // On nettoie le fichier temporaire
  SD.remove(OUTBOX_NEW);
}

void SdOutbox::flushIfAny(BleLink& ble) {
  if (!_sdOk) return;
  if (!ble.isConnected()) return;

  while (true) {
    File f = SD.open(OUTBOX_FILE, FILE_READ);
    if (!f) return;

    String line;
    bool hasLine = readFirstLine(f, line);
    f.close();

    if (!hasLine) return; // Fichier vide, on s'arrête

    uint32_t seq = 0;
    if (!parseSeq_(line.c_str(), seq)) {
      // Ligne corrompue : on la supprime et on passe à la suivante
      removeFirstLine_();
      continue;
    }

    // Tentative d'envoi
    bool started = ble.sendRecord(seq, line.c_str());
    if (!started) return; // Échec d'envoi, on arrête le flush pour cette fois

    // Attente de l'ACK
    unsigned long start = millis();
    while (!ble.ackReceived() && !ble.ackTimedOut()) {
      ble.poll();
      // Petite sécurité pour ne pas rester bloqué indéfiniment
      if (millis() - start > (ACK_TIMEOUT_MS + 100)) break;
    }

    if (ble.ackReceived()) {
      ble.resetAck();
      removeFirstLine_(); // Envoi réussi, on dépile la ligne
      continue;
    } else {
      ble.resetAck();
      return; // Timeout, on réessaiera plus tard
    }
  }
}