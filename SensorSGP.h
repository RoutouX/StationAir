#pragma once
#include <Arduino.h>
#include "Adafruit_SGP30.h"

class SensorSGP {
public:
  bool begin();
  bool sampleEco2(uint16_t& eco2);

private:
  Adafruit_SGP30 sgp;
};
