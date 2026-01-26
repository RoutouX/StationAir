#include <Wire.h>
#include "RTClib.h"

#include "Config.h"
#include "TimeUtil.h"
#include "Median.h"

#include "SensorSGP.h"
#include "BleLink.h"
#include "SdOutbox.h"

// ===== Devices =====
RTC_DS1307 rtc;
SensorSGP sensor;
BleLink ble;
SdOutbox outbox;

// ===== Minute buffer (eCO2 only) =====
uint16_t bufEco2[MAX_SAMPLES_PER_MIN];
uint8_t  bufCount = 0;

int lastMinute = -1;
int lastHour = -1;
int lastDay = -1;

unsigned long lastSampleMs = 0;
uint32_t nextSeq = 1;

static void buildRecordLine(char* out, size_t outSz,
                            uint32_t seq, uint32_t unixTs, const char* dateStr,
                            uint16_t eco2m, uint32_t uptimeS) {
  snprintf(out, outSz,
           "%lu;%lu;%s;%u;%lu",
           (unsigned long)seq,
           (unsigned long)unixTs,
           dateStr,
           eco2m,
           (unsigned long)uptimeS);
}

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println("\n=== STATIONAIR (multi-files): MEDIAN eCO2 / MIN + SD OUTBOX + ACK BLE ===");

  Wire.begin();

  // RTC
  if (!rtc.begin()) { Serial.println("❌ RTC HS"); while (1) {} }
  if (!rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // SD
  Serial.print("Init SD... ");
  if (outbox.begin(PIN_SD)) {
    Serial.println("✅ OK SD");
  } else {
    Serial.println("⚠️ SD absente (retry en boucle)");
  }

  // SGP30
  if (!sensor.begin()) { Serial.println("❌ SGP30 HS"); while (1) {} }

  // BLE
  if (!ble.begin()) { Serial.println("❌ BLE ECHEC"); while (1) {} }
  Serial.println("✅ BLE prêt (advertising).");
}

void loop() {
  ble.poll();
  ble.onAckUpdate();

  // SD retry if missing
  outbox.tryReinit(PIN_SD);

  // On connect: flush queue
  if (ble.justConnected()) {
    Serial.print("👋 CONNECTÉ A : ");
    Serial.println(ble.centralAddress());
    outbox.flushIfAny(ble);
  }
  if (ble.justDisconnected()) {
    Serial.println("👋 DÉCONNECTÉ -> mode SD");
  }

  // Sampling every second
  unsigned long nowMs = millis();
  if (nowMs - lastSampleMs < SAMPLE_INTERVAL_MS) return;
  lastSampleMs = nowMs;

  // Sample sensor
  uint16_t eco2 = 0;
  if (!sensor.sampleEco2(eco2)) return;

  DateTime now = rtc.now();
  if (lastMinute < 0) {
    lastMinute = now.minute();
    lastHour = now.hour();
    lastDay = now.day();
  }

  // Push into minute buffer
  if (bufCount < MAX_SAMPLES_PER_MIN) {
    bufEco2[bufCount++] = eco2;
  }

  // minute changed?
  bool minuteChanged = (now.minute() != lastMinute) || (now.hour() != lastHour) || (now.day() != lastDay);
  if (!minuteChanged) return;

  // Median of last minute samples
  uint16_t eco2m = median_u16(bufEco2, bufCount);

  // Record timestamp string (moment we detected change)
  char dateStr[20];
  formatDateStr(dateStr, sizeof(dateStr), now);

  uint32_t unixTs = now.unixtime();
  uint32_t seq = nextSeq++;

  char line[100];
  buildRecordLine(line, sizeof(line), seq, unixTs, dateStr, eco2m, (uint32_t)(millis() / 1000UL));

  // Print each minute median
  Serial.print("📊 MEDIAN eCO2 (1min) | seq=");
  Serial.print(seq);
  Serial.print(" | ");
  Serial.print(dateStr);
  Serial.print(" | eCO2=");
  Serial.print(eco2m);
  Serial.println(" ppm");

  // Reset buffers for new minute
  bufCount = 0;
  lastMinute = now.minute();
  lastHour = now.hour();
  lastDay = now.day();

  // Update live BLE characteristic
  ble.setEco2(eco2m);

  // Send or store
  if (ble.isConnected()) {
    bool ok = ble.sendRecordWithAck(seq, dateStr, line);
    if (ok) {
      Serial.print("📡 BLE sent + ACK | seq=");
      Serial.print(seq);
      Serial.print(" | ");
      Serial.println(dateStr);

      // flush older SD
      outbox.flushIfAny(ble);
    } else {
      Serial.print("⚠️ BLE send failed (no ACK) -> store SD | seq=");
      Serial.println(seq);
      outbox.appendLine(line);
    }
  } else {
    Serial.print("💾 SD store (no BLE) | seq=");
    Serial.print(seq);
    Serial.print(" | ");
    Serial.println(dateStr);
    outbox.appendLine(line);
  }
}
