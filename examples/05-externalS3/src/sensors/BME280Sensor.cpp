#include "sensors/BME280Sensor.h"

static constexpr uint8_t kBme280AddrPrimary = 0x76;
static constexpr uint8_t kBme280AddrSecondary = 0x77;

BME280Sensor::BME280Sensor() : TelemetrySensor("BME280") {}

bool BME280Sensor::initDevice(TwoWire *bus)
{
  uint8_t addr = kBme280AddrPrimary;
  status = bme280.begin(addr, bus);
  if (!status) {
    addr = kBme280AddrSecondary;
    status = bme280.begin(addr, bus);
  }
  if (!status) {
    return initI2CSensor();
  }

  bme280.setSampling(Adafruit_BME280::MODE_FORCED, Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::SAMPLING_X1,
                     Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::FILTER_OFF, Adafruit_BME280::STANDBY_MS_1000);
  Serial.printf("BME280 ready at 0x%02X\n", addr);
  return initI2CSensor();
}

bool BME280Sensor::getMetrics(EnvironmentMetrics *measurement)
{
  if (!isReady()) {
    return false;
  }

  bme280.takeForcedMeasurement();
  measurement->has_temperature = true;
  measurement->has_relative_humidity = true;
  measurement->has_barometric_pressure = true;
  measurement->temperature = bme280.readTemperature();
  measurement->relative_humidity = bme280.readHumidity();
  measurement->barometric_pressure = bme280.readPressure() / 100.0F;
  return true;
}
