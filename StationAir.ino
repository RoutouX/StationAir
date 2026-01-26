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

// ===== Circular buffer =====
static uint16_t ring[MAX_BUFFER_SAMPLES];
static uint16_t ringCount = 0;
static uint16_t ringHead = 0;

static uint16_t work[MAX_BUFFER_SAMPLES];

// ===== Timing =====
unsigned long lastSampleMs = 0;
unsigned long lastReportMs = 0;

// Auto-calculated sample interval
unsigned long SAMPLE_INTERVAL_MS = 1000;

uint32_t nextSeq = 1;

// ===== Ring helpers =====
static void ringPush(uint16_t v) {
  ring[ringHead] = v;
  ringHead = (ringHead + 1) % MAX_BUFFER_SAMPLES;
  if (ringCount < MAX_BUFFER_SAMPLES) ringCount++;
}

static uint16_t ringGetFromEnd(uint16_t idxFromEnd) {
  if (idxFromEnd >= ringCount) idxFromEnd = ringCount - 1;
  int32_t pos = (int32_t)ringHead - 1 - (int32_t)idxFromEnd;
  while (pos < 0) pos += MAX_BUFFER_SAMPLES;
  return ring[(uint16_t)pos];
}

static uint16_t computeMedianLastNSamples(uint16_t N) {
  if (ringCount == 0 || N == 0) return 0;
  if (N > ringCount) N = ringCount;
  if (N > MAX_BUFFER_SAMPLES) N = MAX_BUFFER_SAMPLES;

  for (uint16_t i = 0; i < N; i++) {
    work[i] = ringGetFromEnd(i);
  }

  sort_u16_inplace(work, N);
  return median_u16_from_sorted(work, N);
}

// ===== CSV builder =====
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

  Serial.println("\n=== STATIONAIR : AUTO SAMPLE RATE (REPORT / N) ===");

  Wire.begin();

  // RTC
  if (!rtc.begin()) { Serial.println("❌ RTC HS"); while (1) {} }
  if (!rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Auto compute sample interval
  SAMPLE_INTERVAL_MS = REPORT_INTERVAL_MS / NB_SAMPLES_PER_MEDIAN;
  if (SAMPLE_INTERVAL_MS < 10) SAMPLE_INTERVAL_MS = 10; // safety clamp

  Serial.print("🧮 REPORT_INTERVAL_MS = ");
  Serial.println(REPORT_INTERVAL_MS);
  Serial.print("🧮 NB_SAMPLES_PER_MEDIAN = ");
  Serial.println(NB_SAMPLES_PER_MEDIAN);
  Serial.print("🧮 SAMPLE_INTERVAL_MS = ");
  Serial.println(SAMPLE_INTERVAL_MS);

  // SD
  Serial.print("Init SD... ");
  if (outbox.begin(PIN_SD)) Serial.println("✅ OK SD");
  else Serial.println("⚠️ SD absente");

  // Sensor
  if (!sensor.begin()) { Serial.println("❌ SGP30 HS"); while (1) {} }

  // BLE
  if (!ble.begin()) { Serial.println("❌ BLE ECHEC"); while (1) {} }
  Serial.println("✅ BLE prêt");

  lastSampleMs = millis();
  lastReportMs = millis();
}

void loop() {
  ble.poll();
  ble.onAckUpdate();

  outbox.tryReinit(PIN_SD);

  // BLE connection events
  if (ble.justConnected()) {
    Serial.print("👋 CONNECTÉ A : ");
    Serial.println(ble.centralAddress());
    outbox.flushIfAny(ble);
  }
  if (ble.justDisconnected()) {
    Serial.println("👋 DÉCONNECTÉ -> SD mode");
  }

  unsigned long nowMs = millis();

  // ===== SAMPLE LOOP =====
  if (nowMs - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = nowMs;

    uint16_t eco2 = 0;
    if (sensor.sampleEco2(eco2)) {
      ringPush(eco2);
    }
  }

  // ===== REPORT LOOP =====
  if (nowMs - lastReportMs < REPORT_INTERVAL_MS) return;
  lastReportMs = nowMs;

  uint16_t eco2m = computeMedianLastNSamples(NB_SAMPLES_PER_MEDIAN);

  DateTime now = rtc.now();
  char dateStr[20];
  formatDateStr(dateStr, sizeof(dateStr), now);

  uint32_t unixTs = now.unixtime();
  uint32_t seq = nextSeq++;

  char line[100];
  buildRecordLine(line, sizeof(line), seq, unixTs, dateStr, eco2m, (uint32_t)(nowMs / 1000UL));

  // Log
  Serial.print("📊 REPORT | interval=");
  Serial.print(REPORT_INTERVAL_MS / 1000);
  Serial.print("s | samples=");
  Serial.print(NB_SAMPLES_PER_MEDIAN);
  Serial.print(" | seq=");
  Serial.print(seq);
  Serial.print(" | eCO2(med)=");
  Serial.print(eco2m);
  Serial.println(" ppm");

  // BLE live value
  ble.setEco2(eco2m);

  // BLE send or SD queue
  if (ble.isConnected()) {
    bool ok = ble.sendRecordWithAck(seq, dateStr, line);
    if (ok) {
      Serial.print("📡 BLE ACK OK | seq=");
      Serial.println(seq);
      outbox.flushIfAny(ble);
    } else {
      Serial.print("⚠️ BLE FAIL -> SD | seq=");
      Serial.println(seq);
      outbox.appendLine(line);
    }
  } else {
    Serial.print("💾 SD SAVE | seq=");
    Serial.println(seq);
    outbox.appendLine(line);
  }
}
