@echo off
setlocal

set "PIO=%USERPROFILE%\.platformio\penv\Scripts\pio.exe"

if not exist "%PIO%" (
  echo PlatformIO wurde nicht gefunden: %PIO%
  exit /b 1
)

rem Workaround fuer Windows-Umgebungen, in denen die Zertifikatssperrpruefung
rem bei PlatformIO-Downloads fehlschlaegt. Diese Einstellung gilt nur fuer
rem diesen Prozess und aendert keine globale PlatformIO-Konfiguration.
if "%PLATFORMIO_SETTING_ENABLE_PROXY_STRICT_SSL%"=="" (
  set "PLATFORMIO_SETTING_ENABLE_PROXY_STRICT_SSL=false"
)

"%PIO%" %*
exit /b %ERRORLEVEL%
