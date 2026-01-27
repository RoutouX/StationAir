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
  if (!isReady || !ble.isConnected()) return;
  if (!SD.exists(OUTBOX_FILE)) return;

  File in = SD.open(OUTBOX_FILE, FILE_READ);
  File newf = SD.open(OUTBOX_NEW, FILE_WRITE);
  if (!in || !newf) return;

  Serial.println(">> Flush SD -> BLE");

  bool firstLine = true;

  while (in.available()) {
    String line = in.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    if (firstLine) {
      firstLine = false;
      if (line.startsWith("seq;")) {
        newf.println(line);
        continue;
      } else {
        newf.println("seq;unix;date;eco2_med;uptime_s");
      }
    }

    int p1 = line.indexOf(';');
    if (p1 < 0) { newf.println(line); continue; }
    uint32_t seq = line.substring(0, p1).toInt();

    int p2 = line.indexOf(';', p1 + 1);
    int p3 = line.indexOf(';', p2 + 1);
    if (p2 < 0 || p3 < 0) { newf.println(line); continue; }
    String dateStr = line.substring(p2 + 1, p3);

    bool ok = ble.sendRecordWithAck(seq, dateStr.c_str(), line.c_str());
    if (!ok) {
      newf.println(line);
      break;
    }
  }

  in.close();
  newf.close();

  SD.remove(OUTBOX_FILE);
  File src = SD.open(OUTBOX_NEW, FILE_READ);
  File dst = SD.open(OUTBOX_FILE, FILE_WRITE);
  while (src.available()) dst.write(src.read());
  src.close();
  dst.close();
  SD.remove(OUTBOX_NEW);
}
