#include "WifiManagerSmart.h"

#include <ESPmDNS.h>
#include <WiFi.h>
#include <time.h>

namespace {
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t AP_FALLBACK_AFTER_DISCONNECT_MS = 30000;
}  // namespace

void WifiManagerSmart::begin(ConfigManager& configManager) {
  _configManager = &configManager;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(HOSTNAME);

  const AppConfig config = _configManager->getConfig();
  if (!config.wifiSsid.isEmpty() && connectStation(config)) {
    _apMode = false;
    startMdns();
    startTimeSync();
    return;
  }

  startAccessPoint(false);
}

void WifiManagerSmart::update() {
  if (!_apMode && WiFi.status() != WL_CONNECTED) {
    if (_disconnectSinceMs == 0) {
      _disconnectSinceMs = millis();
      Serial.println("[WiFi] Verbindung verloren, warte vor AP-Fallback.");
    }

    if (millis() - _disconnectSinceMs > AP_FALLBACK_AFTER_DISCONNECT_MS) {
      startAccessPoint(true);
    }
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    _disconnectSinceMs = 0;
    if (!_mdnsStarted) {
      startMdns();
    }
    if (!_timeSyncStarted) {
      startTimeSync();
    }
  }
}

bool WifiManagerSmart::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WifiManagerSmart::isApMode() const {
  return _apMode;
}

String WifiManagerSmart::ipAddress() const {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  if (_apMode) {
    return WiFi.softAPIP().toString();
  }
  return "0.0.0.0";
}

String WifiManagerSmart::statusText() const {
  if (WiFi.status() == WL_CONNECTED && _apMode) {
    return "WLAN + Access Point";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return "WLAN verbunden";
  }
  if (_apMode) {
    return "Access Point aktiv";
  }
  return "Nicht verbunden";
}

bool WifiManagerSmart::connectStation(const AppConfig& config) {
  Serial.printf("[WiFi] Verbinde mit SSID '%s' ...\n", config.wifiSsid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.disconnect(false, false);
  delay(150);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Verbunden, IP: %s\n", WiFi.localIP().toString().c_str());
    _disconnectSinceMs = 0;
    return true;
  }

  Serial.println("[WiFi] Verbindung fehlgeschlagen.");
  return false;
}

void WifiManagerSmart::startAccessPoint(bool keepStationMode) {
  Serial.println("[WiFi] Starte Access Point.");
  WiFi.mode(keepStationMode ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  _apMode = true;
  Serial.printf("[WiFi] AP '%s' aktiv, IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
}

void WifiManagerSmart::startMdns() {
  if (WiFi.status() != WL_CONNECTED || _mdnsStarted) {
    return;
  }

  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    _mdnsStarted = true;
    Serial.println("[mDNS] smartscale.local aktiv.");
  } else {
    Serial.println("[mDNS] Start fehlgeschlagen.");
  }
}

void WifiManagerSmart::startTimeSync() {
  if (WiFi.status() != WL_CONNECTED || _timeSyncStarted) {
    return;
  }

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  _timeSyncStarted = true;
  Serial.println("[Time] NTP-Zeitsynchronisation gestartet.");
}
