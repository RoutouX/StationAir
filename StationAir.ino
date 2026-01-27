#include <Wire.h>
#include "RTClib.h"

#include "Config.h"
#include "TimeUtil.h"
#include "Median.h"

#include "SensorSGP.h"
#include "BleLink.h"
#include "SdOutbox.h"

RTC_DS1307 rtc;
SensorSGP sensor;
BleLink ble;
SdOutbox outbox;

// ===== Buffers =====
static uint16_t ring[MAX_BUFFER_SAMPLES];
static uint16_t ringCount = 0;
static uint16_t ringHead = 0;
static uint16_t work[MAX_BUFFER_SAMPLES];

// ===== Timing =====
unsigned long lastSampleMs = 0;
unsigned long lastReportMs = 0;
unsigned long SAMPLE_INTERVAL_MS = 1000;

// ===== LED blink =====
enum class LedMode { Off, Blink, Solid };
LedMode ledMode = LedMode::Off;
unsigned long ledBlinkStartMs = 0;
unsigned long ledSolidStartMs = 0;

uint32_t nextSeq = 1;

// ===== Helpers =====
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

static void logLedState(const char* state, const char* reason) {
  Serial.print("[LED] ");
  Serial.print(state);
  if (reason && reason[0] != '\0') {
    Serial.print(" - ");
    Serial.print(reason);
  }
  Serial.println();
}

static void setLedOff(const char* reason = nullptr) {
  if (ledMode != LedMode::Off || reason) {
    logLedState("OFF", reason);
  }
  ledMode = LedMode::Off;
  digitalWrite(PIN_STATUS_LED, LOW);
}

static void startLedSolid(const char* reason = nullptr) {
  logLedState("SOLID", reason);
  ledMode = LedMode::Solid;
  ledSolidStartMs = millis();
  digitalWrite(PIN_STATUS_LED, HIGH);
}

static void startLedBlink(const char* reason = nullptr) {
  logLedState("BLINK", reason);
  ledMode = LedMode::Blink;
  ledBlinkStartMs = millis();
}

void updateLed() {
  if (ledMode == LedMode::Solid) {
    digitalWrite(PIN_STATUS_LED, HIGH);
    unsigned long now = millis();
    if (now - ledSolidStartMs >= LED_SOLID_DURATION_MS) {
      setLedOff("solid end");
    }
    return;
  }
  if (ledMode != LedMode::Blink) return;

  unsigned long now = millis();

  // Blink ~4Hz
  bool state = ((now / 125) % 2) == 0;
  digitalWrite(PIN_STATUS_LED, state ? HIGH : LOW);

  if (now - ledBlinkStartMs >= LED_BLINK_DURATION_MS) {
    setLedOff("blink end");
  }
}

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

  Serial.println("\n=== STATIONAIR : AUTO SAMPLE + LED BLE BLINK ===");

  pinMode(PIN_STATUS_LED, OUTPUT);
  setLedOff("init");

  Wire.begin();

  if (!rtc.begin()) { Serial.println("ERR RTC HS"); while (1) {} }
  if (!rtc.isrunning()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Auto sample interval
  SAMPLE_INTERVAL_MS = REPORT_INTERVAL_MS / NB_SAMPLES_PER_MEDIAN;
  if (SAMPLE_INTERVAL_MS < 10) SAMPLE_INTERVAL_MS = 10;

  Serial.print("REPORT_INTERVAL_MS="); Serial.println(REPORT_INTERVAL_MS);
  Serial.print("NB_SAMPLES_PER_MEDIAN="); Serial.println(NB_SAMPLES_PER_MEDIAN);
  Serial.print("SAMPLE_INTERVAL_MS="); Serial.println(SAMPLE_INTERVAL_MS);

  Serial.print("Init SD... ");
  if (outbox.begin(PIN_SD)) Serial.println("OK");
  else Serial.println("SD absente");

  if (!sensor.begin()) { Serial.println("ERR SGP30 HS"); while (1) {} }
  if (!ble.begin())    { Serial.println("ERR BLE HS");  while (1) {} }

  lastSampleMs = millis();
  lastReportMs = millis();
}

void loop() {
  ble.poll();
  ble.onAckUpdate();
  updateLed();

  outbox.tryReinit(PIN_SD);

  if (ble.justConnected()) {
    Serial.print(">> CONNECTED: ");
    Serial.println(ble.centralAddress());
    outbox.flushIfAny(ble);
  }

  unsigned long nowMs = millis();

  // SAMPLE
  if (nowMs - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = nowMs;
    uint16_t eco2 = 0;
    if (sensor.sampleEco2(eco2)) ringPush(eco2);
  }

  // REPORT
  if (nowMs - lastReportMs < REPORT_INTERVAL_MS) return;
  lastReportMs = nowMs;

  uint16_t eco2m = computeMedianLastNSamples(NB_SAMPLES_PER_MEDIAN);

  DateTime now = rtc.now();
  char dateStr[20];
  formatDateStr(dateStr, sizeof(dateStr), now);

  uint32_t seq = nextSeq++;

  char line[100];
  buildRecordLine(line, sizeof(line), seq, now.unixtime(), dateStr, eco2m, nowMs / 1000UL);

  Serial.print(">> REPORT eCO2(med)=");
  Serial.print(eco2m);
  Serial.print(" ppm | seq=");
  Serial.println(seq);

  ble.setEco2(eco2m);

  if (ble.isConnected()) {
    Serial.print(">> BLE send try seq=");
    Serial.println(seq);
    startLedBlink("BLE send"); // blink 2s for BLE send
    bool ok = ble.sendRecordWithAck(seq, dateStr, line);
    if (ok) {
      Serial.println(">> BLE OK (blink will stop after 2s)");
      outbox.flushIfAny(ble);
    } else {
      Serial.println("!! BLE FAIL -> SD");
      Serial.println(">> SD append (BLE fail)");
      outbox.appendLine(line);
      startLedSolid("SD write (BLE fail)");
    }
  } else {
    Serial.println(">> SD append (no BLE)");
    outbox.appendLine(line);
    startLedSolid("SD write (no BLE)");
  }
}
