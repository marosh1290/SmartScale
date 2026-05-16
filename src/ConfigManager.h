#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct AppConfig {
  String deviceName;
  String wifiSsid;
  String wifiPassword;
  long tareOffset;
  float calibrationFactor;
  uint16_t sampleCount;
  uint32_t measureIntervalMs;
  String unit;
  float emptyKegWeightKg;
  float maxKegVolumeL;
  String serverUrl;
  uint16_t serverPort;
  String deviceToken;
  bool serverSyncEnabled;
  bool tareCompleted;
  bool calibrationCompleted;
};

class ConfigManager {
 public:
  bool begin();

  const AppConfig& config() const;
  AppConfig getConfig() const;
  void setConfig(const AppConfig& config);

  bool save();
  bool resetDefaults();
  void loadDefaults(AppConfig& target) const;

 private:
  void clampConfig(AppConfig& config) const;

  Preferences _preferences;
  AppConfig _config;
  bool _started = false;
};
