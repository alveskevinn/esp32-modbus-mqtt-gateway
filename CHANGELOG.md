# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the project uses [Semantic Versioning](https://semver.org/).

## [1.0.0] - 2026-11-03

### Added
- Modbus RTU master over RS-485 with DE/RE control, retries and per-request timeout.
- Register map with u16/s16/u32/s32/float32 decoding, word order and scale.
- Block planner that batches contiguous registers into as few FC03/FC04 reads as possible.
- MQTT telemetry, retained `status` (LWT) and `info` topics, and a `cmd` topic (interval, reboot, portal, OTA).
- Captive portal for Wi-Fi, MQTT and Modbus settings, persisted in NVS.
- ArduinoOTA on the LAN and HTTP(S) pull OTA.
- Optional deep sleep cycle.
- Native unit tests and a CI pipeline that builds ESP32 and ESP32-C3 targets.
