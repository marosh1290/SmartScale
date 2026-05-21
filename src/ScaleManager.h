#pragma once

#include <Arduino.h>
#include <HX711.h>

#include "ConfigManager.h"

struct ScaleSnapshot {
  long raw;
  long tareOffset;
  float calibrationFactor;
  double weightKg;
  bool hx711Ready;
  uint32_t lastMeasurementMs;
  uint8_t bufferedSamples;
  bool calibrationAllowed;
};

struct ScaleOperationResult {
  bool success;
  String message;
  long rawAverage;
  long tareOffset;
  float calibrationFactor;
};

class ScaleManager {
 public:
  static constexpr uint8_t HX711_DOUT_PIN = 4;
  static constexpr uint8_t HX711_SCK_PIN = 5;

  ScaleManager();

  void begin(ConfigManager& configManager);
  void update();
  void applyConfig();

  ScaleSnapshot snapshot() const;
  String calibrationBlockReason() const;
  ScaleOperationResult tare(uint8_t samples = 0);
  ScaleOperationResult calibrate(float knownWeightKg, uint8_t samples = 0);

 private:
  static constexpr uint8_t MAX_AVERAGE_SAMPLES = 50;

  bool readRawWithTimeout(long& value, uint32_t timeoutMs);
  bool collectAverage(uint8_t samples, long& average, uint32_t perSampleTimeoutMs);
  uint8_t configuredSampleCount(uint8_t minimum = 1) const;
  void pushRawLocked(long raw);
  double averageRawLocked() const;

  HX711 _hx711;
  ConfigManager* _configManager = nullptr;
  bool _begun = false;
  volatile bool _operationActive = false;

  long _raw = 0;
  double _averageRaw = 0.0;
  double _weightKg = 0.0;
  bool _hx711Ready = false;
  uint32_t _lastMeasureAt = 0;
  uint32_t _lastMeasurementMs = 0;

  long _rawBuffer[MAX_AVERAGE_SAMPLES] = {0};
  uint8_t _bufferSize = 1;
  uint8_t _bufferIndex = 0;
  uint8_t _bufferCount = 0;

  mutable portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
