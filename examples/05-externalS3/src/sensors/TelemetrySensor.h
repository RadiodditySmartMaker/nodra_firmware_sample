#pragma once

#include "telemetry/EnvironmentMetrics.h"
#include <Arduino.h>
#include <Wire.h>

class TelemetrySensor
{
protected:
  TelemetrySensor(const char *name) : sensorName(name) {}

  const char *sensorName;
  bool status = false;
  bool initialized = false;

  bool initI2CSensor()
  {
    if (!status) {
      Serial.printf("Can't connect to %s sensor\n", sensorName);
    } else {
      Serial.printf("Opened %s sensor on i2c bus\n", sensorName);
    }
    initialized = true;
    return status;
  }

public:
  virtual ~TelemetrySensor() {}

  const char *name() const { return sensorName; }
  bool isReady() const { return status && initialized; }

  virtual bool initDevice(TwoWire *bus) = 0;
  virtual bool getMetrics(EnvironmentMetrics *measurement) = 0;
};
