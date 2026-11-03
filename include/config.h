// Board wiring and compile-time defaults. Runtime settings (MQTT, slave id,
// baud rate, intervals) are edited in the captive portal and stored in NVS.
#pragma once

#define FW_VERSION "1.0.0"

// --- RS-485 transceiver (MAX485 / MAX3485 / SP3485) -------------------------
#ifndef RS485_RX_GPIO
#define RS485_RX_GPIO 16  // RO
#endif
#ifndef RS485_TX_GPIO
#define RS485_TX_GPIO 17  // DI
#endif
// DE and /RE tied together. Use -1 for auto-direction modules.
#ifndef RS485_DE_GPIO
#define RS485_DE_GPIO 4
#endif

// Hold LOW during boot to force the configuration portal (BOOT button).
#ifndef PORTAL_BUTTON_GPIO
#define PORTAL_BUTTON_GPIO 0
#endif

// --- Defaults used on first boot --------------------------------------------
#define DEFAULT_MQTT_PORT 1883
#define DEFAULT_BASE_TOPIC "devskin/gateways"
#define DEFAULT_SLAVE_ID 1
#define DEFAULT_BAUD 9600
#define DEFAULT_POLL_S 10
#define DEFAULT_SLEEP_S 0  // 0 = always on; >0 = read, publish, deep sleep

#define PORTAL_AP_PREFIX "MB-Gateway-"
#define PORTAL_TIMEOUT_S 180

// Gap (in registers) worth reading through to save a request.
#define MODBUS_MERGE_GAP 8
#define MODBUS_TIMEOUT_MS 1000
#define MODBUS_RETRIES 2
