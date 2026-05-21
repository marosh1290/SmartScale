#include "WebServerManager.h"

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <LittleFS.h>

#include "KegMetrics.h"

namespace {
template <typename TDocument>
String jsonString(TDocument& document) {
  String output;
  serializeJson(document, output);
  return output;
}

String readString(JsonObject object, const char* key, const String& fallback, bool allowEmpty) {
  if (object[key].isNull()) {
    return fallback;
  }

  String value = object[key].as<String>();
  value.trim();
  if (!allowEmpty && value.isEmpty()) {
    return fallback;
  }
  return value;
}

void restartTask(void*) {
  vTaskDelay(pdMS_TO_TICKS(800));
  ESP.restart();
}
}  // namespace

void WebServerManager::begin(ConfigManager& configManager,
                             ScaleManager& scaleManager,
                             WifiManagerSmart& wifiManager,
                             ServerClient& serverClient) {
  _configManager = &configManager;
  _scaleManager = &scaleManager;
  _wifiManager = &wifiManager;
  _serverClient = &serverClient;

  registerRoutes();
  _server.begin();
  Serial.println("[Web] HTTP-Server gestartet.");
}

void WebServerManager::registerRoutes() {
  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleStatus(request);
  });

  _server.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetSettings(request);
  });

  _server.on("/api/tare", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleTare(request);
  });

  AsyncCallbackJsonWebHandler* calibrateHandler =
      new AsyncCallbackJsonWebHandler("/api/calibrate", [this](AsyncWebServerRequest* request,
                                                               JsonVariant& json) {
        handleCalibrate(request, json);
      });
  calibrateHandler->setMethod(HTTP_POST);
  _server.addHandler(calibrateHandler);

  AsyncCallbackJsonWebHandler* settingsHandler =
      new AsyncCallbackJsonWebHandler("/api/settings", [this](AsyncWebServerRequest* request,
                                                              JsonVariant& json) {
        handleSaveSettings(request, json);
      });
  settingsHandler->setMethod(HTTP_POST);
  _server.addHandler(settingsHandler);

  _server.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleRestart(request);
  });

  _server.on("/api/factory-reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleFactoryReset(request);
  });

  _server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(500, "text/plain", "index.html fehlt im LittleFS.");
    }
  });

  _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  _server.onNotFound([](AsyncWebServerRequest* request) {
    if (request->url().startsWith("/api/")) {
      request->send(404, "application/json", "{\"ok\":false,\"message\":\"Endpoint nicht gefunden.\"}");
      return;
    }

    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "Nicht gefunden.");
    }
  });
}

void WebServerManager::handleStatus(AsyncWebServerRequest* request) {
  const AppConfig config = _configManager->getConfig();
  const ScaleSnapshot scale = _scaleManager->snapshot();
  const KegMetrics keg = calculateKegMetrics(scale.weightKg, config);
  const String calibrationReason = _scaleManager->calibrationBlockReason();

  DynamicJsonDocument document(1536);
  document["deviceName"] = config.deviceName;
  document["weightKg"] = scale.weightKg;
  document["beerWeightKg"] = keg.beerWeightKg;
  document["fillLiters"] = keg.fillLiters;
  document["fillPercent"] = keg.fillPercent;
  document["emptyKegWeightKg"] = config.emptyKegWeightKg;
  document["maxKegVolumeL"] = config.maxKegVolumeL;
  document["raw"] = scale.raw;
  document["tareOffset"] = scale.tareOffset;
  document["calibrationFactor"] = scale.calibrationFactor;
  document["hx711Ready"] = scale.hx711Ready;
  document["bufferedSamples"] = scale.bufferedSamples;
  document["calibrationAllowed"] = scale.calibrationAllowed;
  document["calibrationBlockReason"] = calibrationReason;
  document["tareCompleted"] = config.tareCompleted;
  document["calibrationCompleted"] = config.calibrationCompleted;
  document["setupComplete"] = config.tareCompleted && config.calibrationCompleted;
  document["wifiConnected"] = _wifiManager->isConnected();
  document["wifiStatus"] = _wifiManager->statusText();
  document["ip"] = _wifiManager->ipAddress();
  document["apMode"] = _wifiManager->isApMode();
  document["unit"] = config.unit;
  document["serverSyncEnabled"] = config.serverSyncEnabled;
  document["serverUploadInProgress"] = _serverClient->uploadInProgress();
  document["serverConnected"] = _serverClient->isConnected();
  document["lastServerStatus"] = _serverClient->lastHttpStatus();
  document["lastServerError"] = _serverClient->lastError();
  document["lastMeasurementMs"] = scale.lastMeasurementMs;
  document["uptimeMs"] = millis();

  request->send(200, "application/json", jsonString(document));
}

void WebServerManager::handleGetSettings(AsyncWebServerRequest* request) {
  const AppConfig config = _configManager->getConfig();

  DynamicJsonDocument document(1536);
  document["deviceName"] = config.deviceName;
  document["wifiSsid"] = config.wifiSsid;
  document["wifiPassword"] = "";
  document["wifiPasswordConfigured"] = !config.wifiPassword.isEmpty();
  document["tareOffset"] = config.tareOffset;
  document["calibrationFactor"] = config.calibrationFactor;
  document["sampleCount"] = config.sampleCount;
  document["measureIntervalMs"] = config.measureIntervalMs;
  document["unit"] = config.unit;
  document["emptyKegWeightKg"] = config.emptyKegWeightKg;
  document["maxKegVolumeL"] = config.maxKegVolumeL;
  document["serverUrl"] = config.serverUrl;
  document["serverPort"] = config.serverPort;
  document["deviceToken"] = config.deviceToken;
  document["serverSyncEnabled"] = config.serverSyncEnabled;
  document["tareCompleted"] = config.tareCompleted;
  document["calibrationCompleted"] = config.calibrationCompleted;

  request->send(200, "application/json", jsonString(document));
}

void WebServerManager::handleSaveSettings(AsyncWebServerRequest* request, JsonVariant& json) {
  JsonObject object = json.as<JsonObject>();
  if (object.isNull()) {
    sendSimple(request, 400, false, "Ungültiges JSON.");
    return;
  }

  AppConfig config = _configManager->getConfig();
  config.deviceName = readString(object, "deviceName", config.deviceName, false);
  config.wifiSsid = readString(object, "wifiSsid", config.wifiSsid, true);

  if (!object["wifiPassword"].isNull()) {
    String password = object["wifiPassword"].as<String>();
    if (!password.isEmpty()) {
      config.wifiPassword = password;
    }
  }

  if (!object["sampleCount"].isNull()) {
    config.sampleCount = object["sampleCount"].as<uint16_t>();
  }
  if (!object["measureIntervalMs"].isNull()) {
    config.measureIntervalMs = object["measureIntervalMs"].as<uint32_t>();
  }
  config.unit = readString(object, "unit", config.unit, false);
  if (!object["emptyKegWeightKg"].isNull()) {
    config.emptyKegWeightKg = object["emptyKegWeightKg"].as<float>();
  }
  if (!object["maxKegVolumeL"].isNull()) {
    config.maxKegVolumeL = object["maxKegVolumeL"].as<float>();
  }
  config.serverUrl = readString(object, "serverUrl", config.serverUrl, true);
  if (!object["serverPort"].isNull()) {
    config.serverPort = object["serverPort"].as<uint16_t>();
  }
  config.deviceToken = readString(object, "deviceToken", config.deviceToken, true);

  if (!object["serverSyncEnabled"].isNull()) {
    config.serverSyncEnabled = object["serverSyncEnabled"].as<bool>();
  }

  _configManager->setConfig(config);
  if (!_configManager->save()) {
    sendSimple(request, 500, false, "Einstellungen konnten nicht gespeichert werden.");
    return;
  }

  _scaleManager->applyConfig();
  _serverClient->configure(_configManager->config());
  sendSimple(request, 200, true, "Einstellungen gespeichert. WLAN-Änderungen werden nach einem Neustart aktiv.");
}

void WebServerManager::handleCalibrate(AsyncWebServerRequest* request, JsonVariant& json) {
  JsonObject object = json.as<JsonObject>();
  if (object.isNull() || object["knownWeightKg"].isNull()) {
    sendSimple(request, 400, false, "Referenzgewicht fehlt.");
    return;
  }

  const float knownWeightKg = object["knownWeightKg"].as<float>();
  sendScaleResult(request, _scaleManager->calibrate(knownWeightKg));
}

void WebServerManager::handleTare(AsyncWebServerRequest* request) {
  sendScaleResult(request, _scaleManager->tare());
}

void WebServerManager::handleRestart(AsyncWebServerRequest* request) {
  sendSimple(request, 200, true, "Neustart wird ausgeführt.");
  scheduleRestart();
}

void WebServerManager::handleFactoryReset(AsyncWebServerRequest* request) {
  if (!_configManager->resetDefaults()) {
    sendSimple(request, 500, false, "Werkseinstellungen konnten nicht gespeichert werden.");
    return;
  }

  sendSimple(request, 200, true, "Werkseinstellungen gespeichert. Neustart wird ausgeführt.");
  scheduleRestart();
}

void WebServerManager::sendSimple(AsyncWebServerRequest* request,
                                  int code,
                                  bool ok,
                                  const String& message) {
  DynamicJsonDocument document(256);
  document["ok"] = ok;
  document["message"] = message;
  request->send(code, "application/json", jsonString(document));
}

void WebServerManager::sendScaleResult(AsyncWebServerRequest* request,
                                       const ScaleOperationResult& result) {
  DynamicJsonDocument document(384);
  document["ok"] = result.success;
  document["message"] = result.message;
  document["rawAverage"] = result.rawAverage;
  document["tareOffset"] = result.tareOffset;
  document["calibrationFactor"] = result.calibrationFactor;
  request->send(result.success ? 200 : 400, "application/json", jsonString(document));
}

void WebServerManager::scheduleRestart() {
  xTaskCreate(restartTask, "restartTask", 2048, nullptr, 1, nullptr);
}
