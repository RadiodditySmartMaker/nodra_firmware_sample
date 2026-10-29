#pragma once

#include "sensors/TelemetrySensor.h"
#include <Adafruit_VEML7700.h>

class VEML7700Sensor : public TelemetrySensor
{
private:
  static constexpr float kMaxRes = 0.0036F;
  static constexpr float kGainMax = 2.0F;
  static constexpr float kItMax = 800.0F;

  Adafruit_VEML7700 veml7700;

  float getResolution();
  float computeLux(uint16_t rawAls, bool corrected);

public:
  VEML7700Sensor();
  bool initDevice(TwoWire *bus) override;
  bool getMetrics(EnvironmentMetrics *measurement) override;
};
