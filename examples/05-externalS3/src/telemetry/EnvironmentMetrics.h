#pragma once

#include <stdint.h>

// Mirrors meshtastic.EnvironmentMetrics field numbers from telemetry.proto.
struct EnvironmentMetrics {
  bool has_temperature = false;
  float temperature = 0;

  bool has_relative_humidity = false;
  float relative_humidity = 0;

  bool has_barometric_pressure = false;
  float barometric_pressure = 0;

  bool has_gas_resistance = false;
  float gas_resistance = 0;

  bool has_voltage = false;
  float voltage = 0;

  bool has_current = false;
  float current = 0;

  bool has_iaq = false;
  uint32_t iaq = 0;

  bool has_distance = false;
  float distance = 0;

  bool has_lux = false;
  float lux = 0;

  bool has_white_lux = false;
  float white_lux = 0;

  bool has_ir_lux = false;
  float ir_lux = 0;

  bool has_uv_lux = false;
  float uv_lux = 0;

  bool has_wind_direction = false;
  uint32_t wind_direction = 0;

  bool has_wind_speed = false;
  float wind_speed = 0;

  bool has_weight = false;
  float weight = 0;

  bool has_wind_gust = false;
  float wind_gust = 0;

  bool has_wind_lull = false;
  float wind_lull = 0;

  bool has_radiation = false;
  float radiation = 0;

  bool has_rainfall_1h = false;
  float rainfall_1h = 0;

  bool has_rainfall_24h = false;
  float rainfall_24h = 0;

  bool has_soil_moisture = false;
  uint32_t soil_moisture = 0;

  bool has_soil_temperature = false;
  float soil_temperature = 0;
};
