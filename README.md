# SmartScale ESP32 Firmware

Lokale Firmware fuer eine ESP32-basierte SmartScale mit HX711, Waegezelle und Webinterface. Die Waage funktioniert ohne Server. Wenn Server-Sync aktiviert wird, verbindet sie sich Plaato-Keg-kompatibel per TCP mit einem frei konfigurierbaren Endpunkt. Entwickelt wurde die Integration fuer Bierwand/Open-Plaato-Keg; als Default ist `devices.bierwand.de:1234` hinterlegt.

## Hardware

- ESP32 DevKit C V4
- HX711 Waegezellenverstaerker
- Waegezelle ueber RJ9 / 4P4C Anschluss
- USB-C Stromversorgung am ESP32

## Verdrahtung

| HX711 | ESP32 DevKit C V4 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| DT / DOUT | GPIO4 |
| SCK | GPIO5 |

## Build und Flash

Wenn `pio` im PATH liegt:

```powershell
pio run
pio run -t upload --upload-port <PORT>
pio run -t uploadfs --upload-port <PORT>
pio device monitor -b 115200 --port <PORT>
```

Dieses Projekt enthaelt Wrapper fuer typische lokale PlatformIO-Installationen:

```powershell
.\scripts\pio.cmd run
.\scripts\pio.cmd run -t upload --upload-port <PORT>
.\scripts\pio.cmd run -t uploadfs --upload-port <PORT>
.\scripts\pio.cmd device monitor -b 115200 --port <PORT>
```

Der Wrapper setzt fuer diesen Prozess `PLATFORMIO_SETTING_ENABLE_PROXY_STRICT_SSL=false`, falls Windows/PlatformIO bei Downloads mit `HTTPClientError` oder `CRYPT_E_NO_REVOCATION_CHECK` aussteigt.
Ersetze `<PORT>` durch den seriellen Port deines ESP32, zum Beispiel `COMx` unter Windows oder `/dev/ttyUSB0` unter Linux.

## Erstinbetriebnahme

1. Firmware flashen.
2. LittleFS-Webinterface hochladen.
3. Wenn keine WLAN-Daten gespeichert sind, startet `SmartScale-Setup`.
4. Mit Passwort `smartscale123` verbinden.
5. `http://192.168.4.1` oeffnen.
6. WLAN-Daten speichern und Neustart ausloesen.

Nach erfolgreicher WLAN-Verbindung ist das Webinterface zusaetzlich unter `http://smartscale.local` erreichbar, sofern mDNS im Netzwerk funktioniert.

## Webinterface

Das Dashboard zeigt Gewicht, Fuellstand in Prozent, Fuellstand in Liter, Biergewicht, Rohwert, HX711-Status, WLAN und Server-Sync.

Beim Erst-Setup sind Tarieren und Kalibrieren direkt im Dashboard sichtbar. Nach dem ersten erfolgreichen Tarieren und Kalibrieren sind diese Aktionen nur noch im Menue `Einstellungen` sichtbar.

Die Kalibrierung ist freigeschaltet, sobald der HX711 Werte liefert.

## Tarieren

1. Waage entlasten oder leeren Aufbau aufstellen.
2. `Tarieren` druecken.
3. Der gemittelte Rohwert wird als `tareOffset` dauerhaft in NVS gespeichert.

Nach Abschluss erscheint im Webinterface eine Rueckmeldung mit dem gespeicherten Rohwert.

## Kalibrieren

1. Zuerst tarieren.
2. Bekanntes Referenzgewicht auflegen.
3. Referenzgewicht in kg eintragen, zum Beispiel `1.000`.
4. `Kalibrieren` druecken.

Berechnung:

```text
calibrationFactor = (rawAverage - tareOffset) / knownWeightKg
weightKg = (raw - tareOffset) / calibrationFactor
```

Die Firmware speichert keinen Faktor, wenn das Referenzgewicht <= 0 ist, der HX711 keine Werte liefert oder die Rohwertaenderung zu klein ist.

## Keg-Funktionen

Unter `Einstellungen` koennen gesetzt werden:

- Leergewicht Keg in kg
- Maximaler Fuellstand in Liter

Daraus berechnet die Firmware lokal:

```text
beerWeightKg = max(0, weightKg - emptyKegWeightKg)
fillLiters = clamp(beerWeightKg, 0, maxKegVolumeL)
fillPercent = clamp(fillLiters / maxKegVolumeL * 100, 0, 100)
```

Die Berechnung nimmt fuer Bier naeherungsweise 1 kg pro Liter an.

## Server-Sync

Server-Sync ist standardmaessig deaktiviert. Alle Funktionen laufen lokal, solange `Server-Sync aktivieren` nicht gesetzt ist.

Die Firmware kann eine beliebige TCP-Verbindung nutzen, sofern der Zielserver das Blynk/Plaato-Keg-Protokoll versteht. Entwickelt und getestet wurde die Anbindung fuer Bierwand/Open-Plaato-Keg.

Fuer die Bierwand/Open-Plaato-Keg-Integration werden benoetigt:

- Plaato-Server: `devices.bierwand.de`
- Plaato-Port: `1234`
- Geraete-Token aus Bierwand

Die Firmware spricht das Blynk/Plaato-Protokoll direkt ueber TCP. Es wird kein Webhook-Secret und keine HMAC-Signatur verwendet. Fuer andere kompatible Server koennen Host, Port und Token im Webinterface angepasst werden.

Protokoll-Ablauf:

```text
TCP connect <server>:<port>
Blynk get_shared_dash mit Geraete-Token
Blynk hardware writes mit Plaato-Keg-Pins
```

Gesendete Plaato-Pins:

- `V48` Fuellstand in Prozent
- `V49` Zapfstatus, aktuell immer `0`
- `V51` Restmenge in Liter
- `V53` HX711-Rohwert
- `V54` Volumen-Rohwert, aktuell Restmenge in Liter
- `V56` Temperatur, aktuell `0.0`
- `V62` Leergewicht Keg
- `V71` Einheit metrisch
- `V74` Bierrest-Einheit `litre`
- `V75` Messmodus Volumen
- `V76` Maximaler Fuellstand
- `V81` WLAN-RSSI
- `V88` Biermodus
- `V93` Firmware-Version

Kommandos, die der Server wie bei Plaato Keg an die TCP-Verbindung sendet, werden lokal umgesetzt:

- `V60` Tara
- `V61` Kalibriergewicht
- `V62` Leergewicht Keg
- `V76` Maximaler Fuellstand
- `V71` metrische Einheit

Wenn Token, WLAN oder TCP-Verbindung fehlen, bleibt die Waage lokal nutzbar und der Status zeigt den letzten Sync-Fehler.

## API

Alle Antworten sind JSON.

- `GET /api/status`
- `POST /api/tare`
- `POST /api/calibrate` mit `{ "knownWeightKg": 1.0 }`
- `GET /api/settings`
- `POST /api/settings`
- `POST /api/restart`
- `POST /api/factory-reset`

## Speicherung

Die Einstellungen werden mit `Preferences` im NVS gespeichert:

- `deviceName`
- `wifiSsid`
- `wifiPassword`
- `tareOffset`
- `calibrationFactor`
- `sampleCount`
- `measureIntervalMs`
- `unit`
- `emptyKegWeightKg`
- `maxKegVolumeL`
- `serverUrl`
- `serverPort`
- `deviceToken`
- `serverSyncEnabled`
- `tareCompleted`
- `calibrationCompleted`

Die internen NVS-Schluessel sind teilweise kuerzer, weil ESP32-NVS-Schluessel auf 15 Zeichen begrenzt sind.

## Fehlerbehebung

- Webinterface nicht erreichbar: Im AP-Modus mit `SmartScale-Setup` verbinden und `http://192.168.4.1` oeffnen.
- `smartscale.local` geht nicht: IP-Adresse aus dem seriellen Monitor verwenden.
- WLAN verbindet nicht: SSID/Passwort speichern, Neustart ausloesen und im seriellen Monitor auf `[WiFi] Verbunden` pruefen.
- HX711 zeigt Fehler: Verdrahtung an GPIO4/GPIO5, 3V3 und GND pruefen.
- Kalibrierung bleibt gesperrt: HX711-Verkabelung und Stromversorgung pruefen.
- Unruhige Werte: `Mittelwerte` erhoehen oder mechanische Befestigung der Waegezelle pruefen.
