#pragma once

#include "sensors/TelemetrySensor.h"
#include <vector>
#include <Wire.h>

// Generic sensor list, same pattern as master_firmware AddI2CSensorTemplate + sensors.
class SensorRegistry
{
public:
  ~SensorRegistry()
  {
    for (TelemetrySensor *sensor : sensors_) {
      delete sensor;
    }
  }

  template <typename T> bool addSensor(TwoWire *bus)
  {
    T *sensor = new T();
    Serial.printf("Init sensor: %s\n", sensor->name());
    if (sensor->initDevice(bus)) {
      sensors_.push_back(sensor);
      return true;
    }
    delete sensor;
    return false;
  }

  bool collectMetrics(EnvironmentMetrics *measurement) const
  {
    bool valid = false;
    for (TelemetrySensor *sensor : sensors_) {
      const bool ok = sensor->getMetrics(measurement);
      valid = valid || ok;
    }
    return valid;
  }

  bool hasSensors() const { return !sensors_.empty(); }
  size_t count() const { return sensors_.size(); }

private:
  std::vector<TelemetrySensor *> sensors_;
};
