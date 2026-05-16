#pragma once

#include <Arduino.h>
#include <WiFiClient.h>

#include "ConfigManager.h"
#include "ScaleManager.h"

class ServerClient {
 public:
  void begin(ConfigManager& configManager, ScaleManager& scaleManager);
  void configure(const AppConfig& config);
  void update(double weightKg, long raw);

  bool uploadInProgress();
  bool isConnected();
  int lastHttpStatus() const;
  String lastError() const;

 private:
  static constexpr uint8_t BLYNK_CMD_RESPONSE = 0;
  static constexpr uint8_t BLYNK_CMD_LOGIN = 2;
  static constexpr uint8_t BLYNK_CMD_PING = 6;
  static constexpr uint8_t BLYNK_CMD_HARDWARE_SYNC = 16;
  static constexpr uint8_t BLYNK_CMD_HARDWARE = 20;
  static constexpr uint8_t BLYNK_CMD_GET_SHARED_DASH = 29;
  static constexpr uint16_t BLYNK_STATUS_OK = 200;

  bool ensureConnected(const AppConfig& config);
  void disconnect(const String& reason);
  void sendLogin(const AppConfig& config);
  void sendMeasurements(const AppConfig& config, double weightKg, long raw);
  void sendPin(const String& pin, const String& value);
  void sendCommand(uint8_t command, const String& body);
  void sendAck(uint16_t msgId);
  void sendPing();
  void handleIncoming();
  void handleCommand(uint8_t command, uint16_t msgId, const String& body);
  void handleHardwareWrite(const String& body);
  void handleHardwareSync(const String& body);
  void applyRemotePin(const String& pin, const String& value);

  static uint16_t readUint16(const uint8_t* data);
  static void writeUint16(uint8_t* data, uint16_t value);
  static String decimalString(double value, uint8_t decimals = 3);

  ConfigManager* _configManager = nullptr;
  ScaleManager* _scaleManager = nullptr;
  WiFiClient _client;

  uint16_t _nextMsgId = 1;
  uint32_t _lastConnectAttemptMs = 0;
  uint32_t _lastMeasurementMs = 0;
  uint32_t _lastPingMs = 0;
  uint32_t _uploadIntervalMs = 30000;
  bool _loggedIn = false;
  int _lastProtocolStatus = 0;
  String _lastError = "";
};
