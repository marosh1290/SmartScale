#include <Arduino.h>
#include <LittleFS.h>

#include "ConfigManager.h"
#include "ScaleManager.h"
#include "ServerClient.h"
#include "WebServerManager.h"
#include "WifiManagerSmart.h"

ConfigManager configManager;
ScaleManager scaleManager;
WifiManagerSmart wifiManager;
ServerClient serverClient;
WebServerManager webServerManager;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== SmartScale Firmware ===");

  if (!configManager.begin()) {
    Serial.println("[Boot] WARNUNG: Konfiguration konnte nicht geladen werden, Defaultwerte werden verwendet.");
  }

  scaleManager.begin(configManager);

  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] Start fehlgeschlagen. Webinterface-Dateien fehlen eventuell.");
  } else {
    Serial.println("[LittleFS] Dateisystem bereit.");
  }

  wifiManager.begin(configManager);
  serverClient.begin(configManager, scaleManager);
  webServerManager.begin(configManager, scaleManager, wifiManager, serverClient);
}

void loop() {
  scaleManager.update();
  wifiManager.update();

  const ScaleSnapshot snapshot = scaleManager.snapshot();
  serverClient.update(snapshot.weightKg, snapshot.raw);

  delay(5);
}
