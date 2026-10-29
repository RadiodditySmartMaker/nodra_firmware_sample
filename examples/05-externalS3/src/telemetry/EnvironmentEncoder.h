#pragma once

#include "telemetry/EnvironmentMetrics.h"
#include <stddef.h>
#include <stdint.h>

size_t encodeEnvironmentTelemetry(uint8_t *out, size_t outCap, const EnvironmentMetrics &metrics);
void logEnvironmentMetrics(const EnvironmentMetrics &metrics);
