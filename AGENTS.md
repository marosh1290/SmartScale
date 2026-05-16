# Codex Guidelines

These guidelines apply to AI coding agents working in this repository.

## Project Scope

SmartScale is an ESP32 firmware project for a local HX711-based keg scale with
a local web interface and optional Plaato/Bierwand-compatible server sync.

Do not change firmware behavior, hardware assumptions, dependencies, build
configuration, or web UI behavior unless the user explicitly asks for it.

## Repository Rules

- Keep changes small and directly related to the user request.
- Prefer the existing Arduino/PlatformIO project structure.
- Do not add cloud dependencies.
- Do not hardcode private WiFi credentials, tokens, secrets, or endpoints beyond
  documented public defaults.
- Keep local-first behavior intact. Server sync must remain optional.
- Preserve support for the ESP32 DevKit C V4, HX711 DOUT on GPIO4, and HX711 SCK
  on GPIO5 unless the user requests different hardware.
- Avoid unrelated formatting churn.

## Build And Verification

Use PlatformIO from the project root:

```powershell
.\scripts\pio.cmd run
```

When changing files under `data`, also verify the LittleFS image:

```powershell
.\scripts\pio.cmd run -t buildfs
```

Only flash hardware when the user asks for it or clearly expects it:

```powershell
.\scripts\pio.cmd run -t upload --upload-port <PORT>
.\scripts\pio.cmd run -t uploadfs --upload-port <PORT>
```

## Firmware Notes

- Measurement must not be blocked by web requests or server communication.
- The web interface must remain reachable even if the HX711 is missing or
  reports errors.
- Settings must be persisted through Preferences/NVS.
- LittleFS is used for the static web interface.
- Bierwand/Plaato sync should use the configured token and stay disabled by
  default unless the user enables it.

## Documentation

When changing user-facing behavior, update the README in the same change. For
metadata-only requests, such as licensing or GitHub preparation, do not touch
firmware, UI, or build files.
