#include "ServerClient.h"

#include <WiFi.h>

#include "KegMetrics.h"

namespace {
constexpr uint32_t CONNECT_RETRY_MS = 10000;
constexpr uint32_t PING_INTERVAL_MS = 15000;
constexpr uint32_t READ_TIMEOUT_MS = 250;
constexpr const char* FIRMWARE_VERSION = "smartscale-plaato-0.3.0";

String pinBody(const String& mode, const String& pin, const String& value) {
  String body;
  body.reserve(mode.length() + pin.length() + value.length() + 2);
  body += mode;
  body += static_cast<char>(0);
  body += pin;
  body += static_cast<char>(0);
  body += value;
  return body;
}

int nextNull(const String& text, int from) {
  for (int i = from; i < static_cast<int>(text.length()); ++i) {
    if (text[i] == '\0') {
      return i;
    }
  }
  return -1;
}

String partUntilNull(const String& text, int from, int until) {
  if (until < from) {
    return "";
  }
  return text.substring(from, until);
}
}  // namespace

void ServerClient::begin(ConfigManager& configManager, ScaleManager& scaleManager) {
  _configManager = &configManager;
  _scaleManager = &scaleManager;
  configure(_configManager->config());
  Serial.println("[ServerClient] Plaato-kompatibler TCP-Client bereit.");
}

void ServerClient::configure(const AppConfig& config) {
  _uploadIntervalMs = config.measureIntervalMs < 5000 ? 30000 : config.measureIntervalMs;
}

void ServerClient::update(double weightKg, long raw) {
  if (_configManager == nullptr || _scaleManager == nullptr) {
    return;
  }

  const AppConfig config = _configManager->getConfig();
  if (!config.serverSyncEnabled) {
    if (_client.connected()) {
      disconnect("Server-Sync deaktiviert.");
    }
    return;
  }
  if (config.deviceToken.isEmpty()) {
    _lastError = "Server-Sync aktiv, aber Geraete-Token fehlt.";
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    disconnect("Kein WLAN fuer Server-Sync.");
    return;
  }
  if (!ensureConnected(config)) {
    return;
  }

  handleIncoming();

  const uint32_t now = millis();
  if (now - _lastPingMs >= PING_INTERVAL_MS) {
    sendPing();
    _lastPingMs = now;
  }

  if (_lastMeasurementMs == 0 || now - _lastMeasurementMs >= _uploadIntervalMs) {
    sendMeasurements(config, weightKg, raw);
    _lastMeasurementMs = now;
  }
}

bool ServerClient::uploadInProgress() {
  return isConnected();
}

bool ServerClient::isConnected() {
  return _client.connected();
}

int ServerClient::lastHttpStatus() const {
  return _lastProtocolStatus;
}

String ServerClient::lastError() const {
  return _lastError;
}

bool ServerClient::ensureConnected(const AppConfig& config) {
  if (_client.connected()) {
    return true;
  }

  _loggedIn = false;
  const uint32_t now = millis();
  if (_lastConnectAttemptMs != 0 && now - _lastConnectAttemptMs < CONNECT_RETRY_MS) {
    return false;
  }
  _lastConnectAttemptMs = now;

  Serial.printf("[ServerClient] Verbinde mit %s:%u ...\n",
                config.serverUrl.c_str(),
                config.serverPort);

  if (!_client.connect(config.serverUrl.c_str(), config.serverPort)) {
    _lastError = "TCP-Verbindung fehlgeschlagen.";
    Serial.printf("[ServerClient] %s\n", _lastError.c_str());
    return false;
  }

  _client.setNoDelay(true);
  _lastError = "";
  _lastProtocolStatus = 0;
  _lastMeasurementMs = 0;
  _lastPingMs = now;
  sendLogin(config);
  Serial.println("[ServerClient] TCP verbunden.");
  return true;
}

void ServerClient::disconnect(const String& reason) {
  if (_client.connected()) {
    _client.stop();
  }
  _loggedIn = false;
  _lastError = reason;
}

void ServerClient::sendLogin(const AppConfig& config) {
  // Plaato Keg firmware identifies itself with get_shared_dash carrying the token.
  sendCommand(BLYNK_CMD_GET_SHARED_DASH, config.deviceToken);
  _loggedIn = true;
}

void ServerClient::sendMeasurements(const AppConfig& config, double weightKg, long raw) {
  if (!_client.connected() || !_loggedIn) {
    return;
  }

  const KegMetrics metrics = calculateKegMetrics(weightKg, config);
  sendPin("51", decimalString(metrics.fillLiters));       // amount_left
  sendPin("48", decimalString(metrics.fillPercent));      // percent_of_beer_left
  sendPin("49", "0");                                    // is_pouring
  sendPin("53", String(raw));                             // weight_raw
  sendPin("54", decimalString(metrics.fillLiters));       // volume_raw
  sendPin("56", "0.0");                                  // keg_temperature
  sendPin("62", decimalString(config.emptyKegWeightKg));  // empty_keg_weight
  sendPin("71", "1");                                    // unit metric
  sendPin("74", "litre");                                // beer_left_unit
  sendPin("75", "2");                                    // measure_unit volume
  sendPin("76", decimalString(config.maxKegVolumeL));     // max_keg_volume
  sendPin("81", String(WiFi.RSSI()));                     // wifi_signal_strength
  sendPin("88", "1");                                    // beer mode
  sendPin("93", FIRMWARE_VERSION);                        // firmware_version
}

void ServerClient::sendPin(const String& pin, const String& value) {
  sendCommand(BLYNK_CMD_HARDWARE, pinBody("vw", pin, value));
}

void ServerClient::sendCommand(uint8_t command, const String& body) {
  if (!_client.connected()) {
    return;
  }

  uint8_t header[5];
  header[0] = command;
  writeUint16(&header[1], _nextMsgId++);
  writeUint16(&header[3], body.length());

  _client.write(header, sizeof(header));
  if (body.length() > 0) {
    _client.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length());
  }
}

void ServerClient::sendAck(uint16_t msgId) {
  if (!_client.connected()) {
    return;
  }

  uint8_t response[5];
  response[0] = BLYNK_CMD_RESPONSE;
  writeUint16(&response[1], msgId);
  writeUint16(&response[3], BLYNK_STATUS_OK);
  _client.write(response, sizeof(response));
}

void ServerClient::sendPing() {
  sendCommand(BLYNK_CMD_PING, "");
}

void ServerClient::handleIncoming() {
  while (_client.connected() && _client.available() >= 5) {
    uint8_t header[5];
    const size_t headerRead = _client.readBytes(header, sizeof(header));
    if (headerRead != sizeof(header)) {
      disconnect("TCP-Lesefehler.");
      return;
    }

    const uint8_t command = header[0];
    const uint16_t msgId = readUint16(&header[1]);
    const uint16_t lengthOrStatus = readUint16(&header[3]);

    if (command == BLYNK_CMD_RESPONSE) {
      _lastProtocolStatus = lengthOrStatus;
      if (lengthOrStatus != BLYNK_STATUS_OK) {
        _lastError = "Blynk/Plaato Antwortstatus " + String(lengthOrStatus);
      }
      continue;
    }

    String body;
    body.reserve(lengthOrStatus);
    const uint32_t startedAt = millis();
    while (body.length() < lengthOrStatus && millis() - startedAt < READ_TIMEOUT_MS) {
      while (_client.available() > 0 && body.length() < lengthOrStatus) {
        body += static_cast<char>(_client.read());
      }
      delay(1);
    }

    if (body.length() != lengthOrStatus) {
      disconnect("TCP-Paket unvollstaendig.");
      return;
    }

    sendAck(msgId);
    handleCommand(command, msgId, body);
  }
}

void ServerClient::handleCommand(uint8_t command, uint16_t, const String& body) {
  if (command == BLYNK_CMD_HARDWARE) {
    handleHardwareWrite(body);
    return;
  }

  if (command == BLYNK_CMD_HARDWARE_SYNC) {
    handleHardwareSync(body);
  }
}

void ServerClient::handleHardwareWrite(const String& body) {
  const int first = nextNull(body, 0);
  const int second = first >= 0 ? nextNull(body, first + 1) : -1;
  if (first < 0 || second < 0) {
    return;
  }

  const String mode = partUntilNull(body, 0, first);
  if (mode != "vw") {
    return;
  }

  const String pin = partUntilNull(body, first + 1, second);
  const String value = body.substring(second + 1);
  applyRemotePin(pin, value);
}

void ServerClient::handleHardwareSync(const String& body) {
  const AppConfig config = _configManager->getConfig();
  ScaleSnapshot snapshot = _scaleManager->snapshot();
  const KegMetrics metrics = calculateKegMetrics(snapshot.weightKg, config);

  int start = 0;
  int end = nextNull(body, start);
  String mode = end >= 0 ? partUntilNull(body, start, end) : body;
  if (mode != "vr") {
    return;
  }

  start = end + 1;
  while (start > 0 && start < static_cast<int>(body.length())) {
    end = nextNull(body, start);
    String pin = end >= 0 ? partUntilNull(body, start, end) : body.substring(start);

    if (pin == "48") sendPin("48", decimalString(metrics.fillPercent));
    else if (pin == "49") sendPin("49", "0");
    else if (pin == "51") sendPin("51", decimalString(metrics.fillLiters));
    else if (pin == "53") sendPin("53", String(snapshot.raw));
    else if (pin == "54") sendPin("54", decimalString(metrics.fillLiters));
    else if (pin == "56") sendPin("56", "0.0");
    else if (pin == "62") sendPin("62", decimalString(config.emptyKegWeightKg));
    else if (pin == "71") sendPin("71", "1");
    else if (pin == "74") sendPin("74", "litre");
    else if (pin == "75") sendPin("75", "2");
    else if (pin == "76") sendPin("76", decimalString(config.maxKegVolumeL));
    else if (pin == "81") sendPin("81", String(WiFi.RSSI()));
    else if (pin == "88") sendPin("88", "1");
    else if (pin == "93") sendPin("93", FIRMWARE_VERSION);

    if (end < 0) {
      break;
    }
    start = end + 1;
  }
}

void ServerClient::applyRemotePin(const String& pin, const String& value) {
  AppConfig config = _configManager->getConfig();
  bool changed = false;

  if (pin == "60" && value == "1") {
    ScaleOperationResult result = _scaleManager->tare();
    _lastError = result.message;
  } else if (pin == "61") {
    float knownWeightKg = value.toFloat();
    if (knownWeightKg > 50.0f) {
      knownWeightKg /= 1000.0f;
    }
    ScaleOperationResult result = _scaleManager->calibrate(knownWeightKg);
    _lastError = result.message;
  } else if (pin == "62") {
    const float parsed = value.toFloat();
    if (parsed >= 0.0f) {
      config.emptyKegWeightKg = parsed;
      changed = true;
    }
  } else if (pin == "76") {
    const float parsed = value.toFloat();
    if (parsed > 0.0f) {
      config.maxKegVolumeL = parsed;
      changed = true;
    }
  } else if (pin == "71") {
    config.unit = "kg";
    changed = true;
  }

  if (changed) {
    _configManager->setConfig(config);
    _configManager->save();
  }

  ScaleSnapshot snapshot = _scaleManager->snapshot();
  sendMeasurements(_configManager->config(), snapshot.weightKg, snapshot.raw);
}

uint16_t ServerClient::readUint16(const uint8_t* data) {
  return (static_cast<uint16_t>(data[0]) << 8) | data[1];
}

void ServerClient::writeUint16(uint8_t* data, uint16_t value) {
  data[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
  data[1] = static_cast<uint8_t>(value & 0xFF);
}

String ServerClient::decimalString(double value, uint8_t decimals) {
  if (!isfinite(value)) {
    value = 0.0;
  }
  return String(value, static_cast<unsigned int>(decimals));
}
