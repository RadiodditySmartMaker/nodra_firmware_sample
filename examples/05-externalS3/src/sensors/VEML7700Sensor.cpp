#include "sensors/VEML7700Sensor.h"

VEML7700Sensor::VEML7700Sensor() : TelemetrySensor("VEML7700") {}

bool VEML7700Sensor::initDevice(TwoWire *bus)
{
  status = veml7700.begin(bus);
  if (status) {
    Serial.println("VEML7700 ready at 0x10");
  }
  return initI2CSensor();
}

float VEML7700Sensor::getResolution()
{
  return kMaxRes * (kItMax / veml7700.getIntegrationTimeValue()) * (kGainMax / veml7700.getGainValue());
}

float VEML7700Sensor::computeLux(uint16_t rawAls, bool corrected)
{
  float lux = getResolution() * rawAls;
  if (corrected) {
    lux = (((6.0135e-13F * lux - 9.3924e-9F) * lux + 8.1488e-5F) * lux + 1.0023F) * lux;
  }
  return lux;
}

bool VEML7700Sensor::getMetrics(EnvironmentMetrics *measurement)
{
  if (!isReady()) {
    return false;
  }

  measurement->has_lux = true;
  measurement->has_white_lux = true;
  measurement->lux = veml7700.readLux(VEML_LUX_AUTO);
  const int16_t white = veml7700.readWhite(true);
  measurement->white_lux = computeLux(static_cast<uint16_t>(white), white > 100);
  return true;
}
