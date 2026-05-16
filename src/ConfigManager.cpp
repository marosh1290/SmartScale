#include "ConfigManager.h"

#include <math.h>

namespace {
constexpr const char* NVS_NAMESPACE = "smartscale";

constexpr const char* KEY_DEVICE_NAME = "deviceName";
constexpr const char* KEY_WIFI_SSID = "wifiSsid";
constexpr const char* KEY_WIFI_PASSWORD = "wifiPass";
constexpr const char* KEY_TARE_OFFSET = "tareOffset";
constexpr const char* KEY_CALIBRATION = "calibFactor";
constexpr const char* KEY_SAMPLE_COUNT = "sampleCount";
constexpr const char* KEY_INTERVAL = "measureMs";
constexpr const char* KEY_UNIT = "unit";
constexpr const char* KEY_EMPTY_KEG_WEIGHT = "emptyKg";
constexpr const char* KEY_MAX_KEG_VOLUME = "maxVolL";
constexpr const char* KEY_SERVER_URL = "serverUrl";
constexpr const char* KEY_SERVER_PORT = "serverPort";
constexpr const char* KEY_DEVICE_TOKEN = "deviceToken";
constexpr const char* KEY_SERVER_SYNC = "serverSync";
constexpr const char* KEY_TARE_DONE = "tareDone";
constexpr const char* KEY_CAL_DONE = "calDone";

constexpr uint16_t MIN_SAMPLE_COUNT = 1;
constexpr uint16_t MAX_SAMPLE_COUNT = 50;
constexpr uint32_t MIN_INTERVAL_MS = 100;
constexpr uint32_t MAX_INTERVAL_MS = 60000;
constexpr float MAX_EMPTY_KEG_WEIGHT_KG = 80.0f;
constexpr float MAX_KEG_VOLUME_L = 200.0f;
}  // namespace

bool ConfigManager::begin() {
  loadDefaults(_config);

  if (!_preferences.begin(NVS_NAMESPACE, false)) {
    Serial.println("[Config] Preferences konnten nicht gestartet werden.");
    return false;
  }

  _started = true;
  _config.deviceName = _preferences.getString(KEY_DEVICE_NAME, _config.deviceName);
  _config.wifiSsid = _preferences.getString(KEY_WIFI_SSID, _config.wifiSsid);
  _config.wifiPassword = _preferences.getString(KEY_WIFI_PASSWORD, _config.wifiPassword);
  _config.tareOffset = _preferences.getLong(KEY_TARE_OFFSET, _config.tareOffset);
  _config.calibrationFactor = _preferences.getFloat(KEY_CALIBRATION, _config.calibrationFactor);
  _config.sampleCount = _preferences.getUShort(KEY_SAMPLE_COUNT, _config.sampleCount);
  _config.measureIntervalMs = _preferences.getUInt(KEY_INTERVAL, _config.measureIntervalMs);
  _config.unit = _preferences.getString(KEY_UNIT, _config.unit);
  if (_preferences.isKey(KEY_EMPTY_KEG_WEIGHT)) {
    _config.emptyKegWeightKg = _preferences.getFloat(KEY_EMPTY_KEG_WEIGHT, _config.emptyKegWeightKg);
  }
  if (_preferences.isKey(KEY_MAX_KEG_VOLUME)) {
    _config.maxKegVolumeL = _preferences.getFloat(KEY_MAX_KEG_VOLUME, _config.maxKegVolumeL);
  }
  _config.serverUrl = _preferences.getString(KEY_SERVER_URL, _config.serverUrl);
  _config.serverPort = _preferences.getUShort(KEY_SERVER_PORT, _config.serverPort);
  _config.deviceToken = _preferences.getString(KEY_DEVICE_TOKEN, _config.deviceToken);
  _config.serverSyncEnabled = _preferences.getBool(KEY_SERVER_SYNC, _config.serverSyncEnabled);
  _config.tareCompleted = _preferences.getBool(KEY_TARE_DONE, _config.tareCompleted);
  _config.calibrationCompleted = _preferences.getBool(KEY_CAL_DONE, _config.calibrationCompleted);

  clampConfig(_config);

  Serial.println("[Config] Konfiguration geladen.");
  return true;
}

const AppConfig& ConfigManager::config() const {
  return _config;
}

AppConfig ConfigManager::getConfig() const {
  return _config;
}

void ConfigManager::setConfig(const AppConfig& config) {
  _config = config;
  clampConfig(_config);
}

bool ConfigManager::save() {
  if (!_started) {
    return false;
  }

  clampConfig(_config);

  _preferences.putString(KEY_DEVICE_NAME, _config.deviceName);
  _preferences.putString(KEY_WIFI_SSID, _config.wifiSsid);
  _preferences.putString(KEY_WIFI_PASSWORD, _config.wifiPassword);
  _preferences.putLong(KEY_TARE_OFFSET, _config.tareOffset);
  _preferences.putFloat(KEY_CALIBRATION, _config.calibrationFactor);
  _preferences.putUShort(KEY_SAMPLE_COUNT, _config.sampleCount);
  _preferences.putUInt(KEY_INTERVAL, _config.measureIntervalMs);
  _preferences.putString(KEY_UNIT, _config.unit);
  _preferences.putFloat(KEY_EMPTY_KEG_WEIGHT, _config.emptyKegWeightKg);
  _preferences.putFloat(KEY_MAX_KEG_VOLUME, _config.maxKegVolumeL);
  _preferences.putString(KEY_SERVER_URL, _config.serverUrl);
  _preferences.putUShort(KEY_SERVER_PORT, _config.serverPort);
  _preferences.putString(KEY_DEVICE_TOKEN, _config.deviceToken);
  _preferences.putBool(KEY_SERVER_SYNC, _config.serverSyncEnabled);
  _preferences.putBool(KEY_TARE_DONE, _config.tareCompleted);
  _preferences.putBool(KEY_CAL_DONE, _config.calibrationCompleted);

  Serial.println("[Config] Konfiguration gespeichert.");
  return true;
}

bool ConfigManager::resetDefaults() {
  if (!_started) {
    return false;
  }

  _preferences.clear();
  loadDefaults(_config);
  return save();
}

void ConfigManager::loadDefaults(AppConfig& target) const {
  target.deviceName = "SmartScale";
  target.wifiSsid = "";
  target.wifiPassword = "";
  target.tareOffset = 0;
  target.calibrationFactor = 21500.0f;
  target.sampleCount = 10;
  target.measureIntervalMs = 1000;
  target.unit = "kg";
  target.emptyKegWeightKg = 0.0f;
  target.maxKegVolumeL = 19.0f;
  target.serverUrl = "devices.bierwand.de";
  target.serverPort = 1234;
  target.deviceToken = "";
  target.serverSyncEnabled = false;
  target.tareCompleted = false;
  target.calibrationCompleted = false;
}

void ConfigManager::clampConfig(AppConfig& config) const {
  config.deviceName.trim();
  if (config.deviceName.isEmpty()) {
    config.deviceName = "SmartScale";
  }

  config.wifiSsid.trim();
  config.serverUrl.trim();
  config.serverUrl.replace("https://", "");
  config.serverUrl.replace("http://", "");
  int slashIndex = config.serverUrl.indexOf('/');
  if (slashIndex >= 0) {
    config.serverUrl = config.serverUrl.substring(0, slashIndex);
  }
  int colonIndex = config.serverUrl.lastIndexOf(':');
  if (colonIndex > 0) {
    String portText = config.serverUrl.substring(colonIndex + 1);
    config.serverUrl = config.serverUrl.substring(0, colonIndex);
    int parsedPort = portText.toInt();
    if (parsedPort > 0 && parsedPort <= 65535) {
      config.serverPort = static_cast<uint16_t>(parsedPort);
    }
  }
  if (config.serverUrl.isEmpty()) {
    config.serverUrl = "devices.bierwand.de";
  }
  if (config.serverPort == 0) {
    config.serverPort = 1234;
  }
  config.deviceToken.trim();
  config.unit.trim();
  config.unit.toLowerCase();
  if (config.unit != "kg" && config.unit != "g") {
    config.unit = "kg";
  }

  if (config.sampleCount < MIN_SAMPLE_COUNT) {
    config.sampleCount = MIN_SAMPLE_COUNT;
  }
  if (config.sampleCount > MAX_SAMPLE_COUNT) {
    config.sampleCount = MAX_SAMPLE_COUNT;
  }

  if (config.measureIntervalMs < MIN_INTERVAL_MS) {
    config.measureIntervalMs = MIN_INTERVAL_MS;
  }
  if (config.measureIntervalMs > MAX_INTERVAL_MS) {
    config.measureIntervalMs = MAX_INTERVAL_MS;
  }

  if (fabs(config.calibrationFactor) < 1.0f) {
    config.calibrationFactor = 21500.0f;
  }

  if (!isfinite(config.emptyKegWeightKg) || config.emptyKegWeightKg < 0.0f) {
    config.emptyKegWeightKg = 0.0f;
  }
  if (config.emptyKegWeightKg > MAX_EMPTY_KEG_WEIGHT_KG) {
    config.emptyKegWeightKg = MAX_EMPTY_KEG_WEIGHT_KG;
  }

  if (!isfinite(config.maxKegVolumeL) || config.maxKegVolumeL <= 0.0f) {
    config.maxKegVolumeL = 19.0f;
  }
  if (config.maxKegVolumeL > MAX_KEG_VOLUME_L) {
    config.maxKegVolumeL = MAX_KEG_VOLUME_L;
  }
}
