#pragma once

#include <Arduino.h>

#include "ConfigManager.h"

class WifiManagerSmart {
 public:
  static constexpr const char* HOSTNAME = "smartscale";
  static constexpr const char* AP_SSID = "SmartScale-Setup";
  static constexpr const char* AP_PASSWORD = "smartscale123";

  void begin(ConfigManager& configManager);
  void update();

  bool isConnected() const;
  bool isApMode() const;
  String ipAddress() const;
  String statusText() const;

 private:
  bool connectStation(const AppConfig& config);
  void startAccessPoint(bool keepStationMode);
  void startMdns();
  void startTimeSync();

  ConfigManager* _configManager = nullptr;
  bool _apMode = false;
  bool _mdnsStarted = false;
  bool _timeSyncStarted = false;
  uint32_t _disconnectSinceMs = 0;
};
