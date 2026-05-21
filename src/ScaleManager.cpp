#include "ScaleManager.h"

#include <math.h>

namespace {
constexpr uint32_t SAMPLE_TIMEOUT_MS = 300;
constexpr long MIN_CALIBRATION_DELTA = 100;
}  // namespace

ScaleManager::ScaleManager() = default;

void ScaleManager::begin(ConfigManager& configManager) {
  _configManager = &configManager;
  _hx711.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
  _begun = true;
  applyConfig();

  Serial.printf("[Scale] HX711 DOUT=GPIO%u SCK=GPIO%u initialisiert.\n", HX711_DOUT_PIN, HX711_SCK_PIN);
}

void ScaleManager::update() {
  if (!_begun || _configManager == nullptr || _operationActive) {
    return;
  }

  const AppConfig config = _configManager->getConfig();
  const uint32_t now = millis();
  if (now - _lastMeasureAt < config.measureIntervalMs) {
    return;
  }
  _lastMeasureAt = now;

  const uint8_t desiredBufferSize = configuredSampleCount();
  portENTER_CRITICAL(&_mux);
  if (_bufferSize != desiredBufferSize) {
    _bufferSize = desiredBufferSize;
    _bufferIndex = 0;
    _bufferCount = 0;
  }
  portEXIT_CRITICAL(&_mux);

  if (!_hx711.is_ready()) {
    portENTER_CRITICAL(&_mux);
    _hx711Ready = false;
    portEXIT_CRITICAL(&_mux);
    return;
  }

  const long raw = _hx711.read();
  const float calibrationFactor =
      fabs(config.calibrationFactor) < 1.0f ? 21500.0f : config.calibrationFactor;

  portENTER_CRITICAL(&_mux);
  _raw = raw;
  _hx711Ready = true;
  _lastMeasurementMs = now;
  pushRawLocked(raw);
  _averageRaw = averageRawLocked();
  _weightKg = (_averageRaw - static_cast<double>(config.tareOffset)) /
              static_cast<double>(calibrationFactor);
  if (fabs(_weightKg) < 0.005) {
    _weightKg = 0.0;
  }
  portEXIT_CRITICAL(&_mux);
}

void ScaleManager::applyConfig() {
  if (_configManager == nullptr) {
    return;
  }

  const uint8_t desiredBufferSize = configuredSampleCount();
  portENTER_CRITICAL(&_mux);
  _bufferSize = desiredBufferSize;
  _bufferIndex = 0;
  _bufferCount = 0;
  _averageRaw = 0.0;
  _weightKg = 0.0;
  portEXIT_CRITICAL(&_mux);
}

ScaleSnapshot ScaleManager::snapshot() const {
  ScaleSnapshot result;
  const AppConfig config = _configManager != nullptr ? _configManager->getConfig() : AppConfig{};

  portENTER_CRITICAL(&_mux);
  result.raw = _raw;
  result.tareOffset = config.tareOffset;
  result.calibrationFactor = config.calibrationFactor;
  result.weightKg = _weightKg;
  result.hx711Ready = _hx711Ready;
  result.lastMeasurementMs = _lastMeasurementMs;
  result.bufferedSamples = _bufferCount;
  portEXIT_CRITICAL(&_mux);

  result.calibrationAllowed = result.hx711Ready;

  return result;
}

String ScaleManager::calibrationBlockReason() const {
  const ScaleSnapshot state = snapshot();

  if (!state.hx711Ready) {
    return "Kalibrierung gesperrt: HX711 liefert aktuell keine Werte.";
  }

  return "";
}

ScaleOperationResult ScaleManager::tare(uint8_t samples) {
  ScaleOperationResult result{false, "", 0, 0, 0.0f};

  if (!_begun || _configManager == nullptr) {
    result.message = "HX711 ist nicht initialisiert.";
    return result;
  }
  if (_operationActive) {
    result.message = "Es läuft bereits ein Waagenvorgang.";
    return result;
  }

  _operationActive = true;
  samples = samples == 0 ? configuredSampleCount() : samples;

  long rawAverage = 0;
  const bool ok = collectAverage(samples, rawAverage, SAMPLE_TIMEOUT_MS);
  _operationActive = false;

  if (!ok) {
    result.message = "Tarieren fehlgeschlagen: HX711 liefert keine Werte.";
    return result;
  }

  AppConfig config = _configManager->getConfig();
  config.tareOffset = rawAverage;
  config.tareCompleted = true;
  _configManager->setConfig(config);
  _configManager->save();

  portENTER_CRITICAL(&_mux);
  _raw = rawAverage;
  _averageRaw = rawAverage;
  _weightKg = 0.0;
  _hx711Ready = true;
  _lastMeasurementMs = millis();
  portEXIT_CRITICAL(&_mux);

  result.success = true;
  result.message = "Tara gespeichert.";
  result.rawAverage = rawAverage;
  result.tareOffset = rawAverage;
  result.calibrationFactor = config.calibrationFactor;
  return result;
}

ScaleOperationResult ScaleManager::calibrate(float knownWeightKg, uint8_t samples) {
  ScaleOperationResult result{false, "", 0, 0, 0.0f};

  if (!_begun || _configManager == nullptr) {
    result.message = "HX711 ist nicht initialisiert.";
    return result;
  }
  const String blockReason = calibrationBlockReason();
  if (!blockReason.isEmpty()) {
    result.message = blockReason;
    return result;
  }
  if (!isfinite(knownWeightKg) || knownWeightKg <= 0.0f) {
    result.message = "Das Referenzgewicht muss größer als 0 kg sein.";
    return result;
  }
  if (_operationActive) {
    result.message = "Es läuft bereits ein Waagenvorgang.";
    return result;
  }

  _operationActive = true;
  samples = samples == 0 ? configuredSampleCount() : samples;

  long rawAverage = 0;
  const bool ok = collectAverage(samples, rawAverage, SAMPLE_TIMEOUT_MS);
  _operationActive = false;

  if (!ok) {
    result.message = "Kalibrierung fehlgeschlagen: HX711 liefert keine Werte.";
    return result;
  }

  AppConfig config = _configManager->getConfig();
  const long delta = rawAverage - config.tareOffset;
  if (labs(delta) < MIN_CALIBRATION_DELTA) {
    result.message = "Kalibrierung fehlgeschlagen: Rohwertänderung ist zu klein.";
    return result;
  }

  const float calibrationFactor = static_cast<float>(delta) / knownWeightKg;
  if (!isfinite(calibrationFactor) || fabs(calibrationFactor) < 1.0f) {
    result.message = "Kalibrierung fehlgeschlagen: ungültiger Kalibrierfaktor.";
    return result;
  }

  config.calibrationFactor = calibrationFactor;
  config.calibrationCompleted = true;
  _configManager->setConfig(config);
  _configManager->save();

  portENTER_CRITICAL(&_mux);
  _raw = rawAverage;
  _averageRaw = rawAverage;
  _weightKg = knownWeightKg;
  _hx711Ready = true;
  _lastMeasurementMs = millis();
  portEXIT_CRITICAL(&_mux);

  result.success = true;
  result.message = "Kalibrierfaktor gespeichert.";
  result.rawAverage = rawAverage;
  result.tareOffset = config.tareOffset;
  result.calibrationFactor = calibrationFactor;
  return result;
}

bool ScaleManager::readRawWithTimeout(long& value, uint32_t timeoutMs) {
  if (!_begun) {
    return false;
  }

  if (!_hx711.wait_ready_timeout(timeoutMs, 1)) {
    portENTER_CRITICAL(&_mux);
    _hx711Ready = false;
    portEXIT_CRITICAL(&_mux);
    return false;
  }

  value = _hx711.read();

  portENTER_CRITICAL(&_mux);
  _hx711Ready = true;
  portEXIT_CRITICAL(&_mux);
  return true;
}

bool ScaleManager::collectAverage(uint8_t samples, long& average, uint32_t perSampleTimeoutMs) {
  samples = constrain(samples, static_cast<uint8_t>(1), MAX_AVERAGE_SAMPLES);

  int64_t sum = 0;
  uint8_t collected = 0;
  for (uint8_t i = 0; i < samples; ++i) {
    long raw = 0;
    if (!readRawWithTimeout(raw, perSampleTimeoutMs)) {
      continue;
    }

    sum += raw;
    collected++;
    delay(10);
  }

  const uint8_t minimumRequired = samples >= 5 ? 3 : 1;
  if (collected < minimumRequired) {
    return false;
  }

  average = static_cast<long>(sum / collected);
  return true;
}

uint8_t ScaleManager::configuredSampleCount(uint8_t minimum) const {
  if (_configManager == nullptr) {
    return minimum;
  }

  uint16_t samples = _configManager->config().sampleCount;
  if (samples < minimum) {
    samples = minimum;
  }
  if (samples > MAX_AVERAGE_SAMPLES) {
    samples = MAX_AVERAGE_SAMPLES;
  }
  return static_cast<uint8_t>(samples);
}

void ScaleManager::pushRawLocked(long raw) {
  if (_bufferSize == 0) {
    _bufferSize = 1;
  }

  _rawBuffer[_bufferIndex] = raw;
  _bufferIndex = (_bufferIndex + 1) % _bufferSize;
  if (_bufferCount < _bufferSize) {
    _bufferCount++;
  }
}

double ScaleManager::averageRawLocked() const {
  if (_bufferCount == 0) {
    return static_cast<double>(_raw);
  }

  int64_t sum = 0;
  for (uint8_t i = 0; i < _bufferCount; ++i) {
    sum += _rawBuffer[i];
  }
  return static_cast<double>(sum) / static_cast<double>(_bufferCount);
}
