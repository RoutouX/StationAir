#include "SensorSGP.h"

bool SensorSGP::begin() {
  return sgp.begin();
}

bool SensorSGP::sampleEco2(uint16_t& eco2) {
  if (!sgp.IAQmeasure()) return false;
  eco2 = sgp.eCO2;
  return true;
}
