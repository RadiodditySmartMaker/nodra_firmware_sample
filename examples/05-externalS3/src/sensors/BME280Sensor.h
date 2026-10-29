#pragma once

#include "sensors/TelemetrySensor.h"
#include <Adafruit_BME280.h>

class BME280Sensor : public TelemetrySensor
{
private:
  Adafruit_BME280 bme280;

public:
  BME280Sensor();
  bool initDevice(TwoWire *bus) override;
  bool getMetrics(EnvironmentMetrics *measurement) override;
};
