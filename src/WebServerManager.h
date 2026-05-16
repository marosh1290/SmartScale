#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#include "ConfigManager.h"
#include "ScaleManager.h"
#include "ServerClient.h"
#include "WifiManagerSmart.h"

class WebServerManager {
 public:
  void begin(ConfigManager& configManager,
             ScaleManager& scaleManager,
             WifiManagerSmart& wifiManager,
             ServerClient& serverClient);

 private:
  void registerRoutes();

  void handleStatus(AsyncWebServerRequest* request);
  void handleGetSettings(AsyncWebServerRequest* request);
  void handleSaveSettings(AsyncWebServerRequest* request, JsonVariant& json);
  void handleCalibrate(AsyncWebServerRequest* request, JsonVariant& json);
  void handleTare(AsyncWebServerRequest* request);
  void handleRestart(AsyncWebServerRequest* request);
  void handleFactoryReset(AsyncWebServerRequest* request);

  void sendSimple(AsyncWebServerRequest* request, int code, bool ok, const String& message);
  void sendScaleResult(AsyncWebServerRequest* request, const ScaleOperationResult& result);
  void scheduleRestart();

  AsyncWebServer _server{80};
  ConfigManager* _configManager = nullptr;
  ScaleManager* _scaleManager = nullptr;
  WifiManagerSmart* _wifiManager = nullptr;
  ServerClient* _serverClient = nullptr;
};
