#include "SdOutbox.h"

bool SdOutbox::begin(int pinSd) {
  isReady = SD.begin(pinSd);
  return isReady;
}

void SdOutbox::tryReinit(int pinSd) {
  if (isReady) return;
  if (SD.begin(pinSd)) {
    Serial.println("✅ SD retrouvée");
    isReady = true;
  }
}

void SdOutbox::writeHeaderIfNeeded(File& f) {
  if (f.size() == 0) {
    f.println("seq;unix;date;eco2_med;uptime_s");
  }
}

void SdOutbox::appendLine(const char* line) {
  if (!isReady) return;

  File f = SD.open(OUTBOX_FILE, FILE_WRITE);
  if (!f) {
    Serial.println("❌ SD: impossible d'ouvrir OUTBOX -> SD désactivée");
    isReady = false;
    return;
  }

  writeHeaderIfNeeded(f);
  f.println(line);
  f.close();
}

void SdOutbox::flushIfAny(BleLink& ble) {
  if (!isReady) return;
  if (!ble.isConnected()) return;
  if (!SD.exists(OUTBOX_FILE)) return;

  File in = SD.open(OUTBOX_FILE, FILE_READ);
  if (!in) return;

  File newf = SD.open(OUTBOX_NEW, FILE_WRITE);
  if (!newf) {
    in.close();
    Serial.println("❌ SD: impossible de créer OUTBOX.NEW");
    return;
  }

  Serial.println("📤 Flush SD -> BLE ...");

  bool firstLine = true;
  bool allDelivered = true;

  while (in.available()) {
    String line = in.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    // Header
    if (firstLine) {
      firstLine = false;
      if (line.startsWith("seq;")) {
        newf.println(line);
        continue;
      } else {
        newf.println("seq;unix;date;eco2_med;uptime_s");
      }
    }

    // Parse: seq;unix;date;eco2;uptime
    int p1 = line.indexOf(';');
    if (p1 < 0) { newf.println(line); continue; }
    uint32_t seq = (uint32_t) line.substring(0, p1).toInt();

    int p2 = line.indexOf(';', p1 + 1);
    int p3 = line.indexOf(';', p2 + 1);
    if (p2 < 0 || p3 < 0) { newf.println(line); continue; }
    String dateStr = line.substring(p2 + 1, p3);

    int p4 = line.indexOf(';', p3 + 1);
    if (p4 < 0) { newf.println(line); continue; }
    uint16_t eco2m = (uint16_t) line.substring(p3 + 1, p4).toInt();

    ble.setEco2(eco2m);

    bool ok = ble.sendRecordWithAck(seq, dateStr.c_str(), line.c_str());
    if (!ok) {
      Serial.print("⚠️ Flush stop (ACK fail) at seq="); Serial.println(seq);

      newf.println(line);
      while (in.available()) {
        String rest = in.readStringUntil('\n');
        rest.trim();
        if (rest.length() > 0) newf.println(rest);
      }
      allDelivered = false;
      break;
    } else {
      Serial.print("✅ Delivered seq="); Serial.println(seq);
    }
  }

  in.close();
  newf.close();

  // Replace OUTBOX.CSV using copy (no SD.rename on your lib)
  SD.remove(OUTBOX_FILE);

  File src = SD.open(OUTBOX_NEW, FILE_READ);
  File dst = SD.open(OUTBOX_FILE, FILE_WRITE);
  if (src && dst) {
    while (src.available()) dst.write(src.read());
  }
  if (src) src.close();
  if (dst) dst.close();

  SD.remove(OUTBOX_NEW);

  if (allDelivered) {
    File check = SD.open(OUTBOX_FILE, FILE_READ);
    if (check) {
      size_t sz = check.size();
      check.close();
      if (sz < 50) SD.remove(OUTBOX_FILE);
    }
    Serial.println("📤 Flush terminé (tout envoyé).");
  }
}
