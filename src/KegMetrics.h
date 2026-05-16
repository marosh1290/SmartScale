#pragma once

#include <Arduino.h>
#include <math.h>

#include "ConfigManager.h"

struct KegMetrics {
  double beerWeightKg;
  double fillLiters;
  double fillPercent;
};

inline double clampDouble(double value, double minimum, double maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

inline KegMetrics calculateKegMetrics(double totalWeightKg, const AppConfig& config) {
  KegMetrics metrics{0.0, 0.0, 0.0};

  if (!isfinite(totalWeightKg)) {
    return metrics;
  }

  const double maxVolumeL = config.maxKegVolumeL > 0.0f ? config.maxKegVolumeL : 19.0;
  metrics.beerWeightKg = totalWeightKg - static_cast<double>(config.emptyKegWeightKg);
  if (metrics.beerWeightKg < 0.0) {
    metrics.beerWeightKg = 0.0;
  }
  metrics.fillLiters = clampDouble(metrics.beerWeightKg, 0.0, maxVolumeL);
  metrics.fillPercent = clampDouble((metrics.fillLiters / maxVolumeL) * 100.0, 0.0, 100.0);

  return metrics;
}
