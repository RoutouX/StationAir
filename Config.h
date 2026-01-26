#pragma once
#include <Arduino.h>

// Pins
constexpr int PIN_SD = 4;

// Sampling / aggregation
constexpr unsigned long SAMPLE_INTERVAL_MS = 1000;
constexpr uint8_t MAX_SAMPLES_PER_MIN = 60;

// SD queue files
static const char* OUTBOX_FILE = "OUTBOX.CSV";
static const char* OUTBOX_NEW  = "OUTBOX.NEW";

// BLE ids
static const char* BLE_LOCAL_NAME = "StationAir";

// Service + characteristics UUIDs (keep yours)
static const char* ENV_SERVICE_UUID = "181A";
static const char* ECO2_CHAR_UUID   = "3001";
static const char* TIME_CHAR_UUID   = "3003";
static const char* SEQ_CHAR_UUID    = "3004";
static const char* ACK_CHAR_UUID    = "3005";
static const char* PAYLOAD_UUID     = "3006";

// Limits
constexpr unsigned long ACK_TIMEOUT_MS = 1500;
