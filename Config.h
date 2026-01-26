#pragma once
#include <Arduino.h>

// ===== PINS =====
constexpr int PIN_SD = 4;

// ===== REPORTING =====

// Intervalle global pour :
// - calcul médiane
// - envoi BLE
// - sauvegarde SD
constexpr unsigned long REPORT_INTERVAL_MS = 20000; // 60s

// Nombre de mesures utilisées pour chaque médiane
constexpr uint16_t NB_SAMPLES_PER_MEDIAN = 20;

// Taille max du buffer circulaire (sécurité RAM)
constexpr uint16_t MAX_BUFFER_SAMPLES = 300;

// ===== SD QUEUE FILES =====
static const char* OUTBOX_FILE = "OUTBOX.CSV";
static const char* OUTBOX_NEW  = "OUTBOX.NEW";

// ===== BLE =====
static const char* BLE_LOCAL_NAME = "StationAir";

static const char* ENV_SERVICE_UUID = "181A";
static const char* ECO2_CHAR_UUID   = "3001";
static const char* TIME_CHAR_UUID   = "3003";
static const char* SEQ_CHAR_UUID    = "3004";
static const char* ACK_CHAR_UUID    = "3005";
static const char* PAYLOAD_UUID     = "3006";

// ===== ACK =====
constexpr unsigned long ACK_TIMEOUT_MS = 1500;
