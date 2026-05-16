param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $PlatformIOArgs
)

$pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"

if (-not (Test-Path -LiteralPath $pio)) {
    Write-Error "PlatformIO wurde nicht gefunden: $pio"
    exit 1
}

# Workaround fuer Windows-Umgebungen, in denen die Zertifikatssperrpruefung
# bei PlatformIO-Downloads fehlschlaegt. Diese Einstellung gilt nur fuer
# diesen Prozess und aendert keine globale PlatformIO-Konfiguration.
if (-not $env:PLATFORMIO_SETTING_ENABLE_PROXY_STRICT_SSL) {
    $env:PLATFORMIO_SETTING_ENABLE_PROXY_STRICT_SSL = "false"
}

& $pio @PlatformIOArgs
exit $LASTEXITCODE
