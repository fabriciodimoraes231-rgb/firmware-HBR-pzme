#pragma once
#include <cstdint>

// ---- Hardware pins ----
constexpr uint8_t LED_PIN = 2;
constexpr uint8_t BOOT_BUTTON_PIN = 0;
constexpr uint8_t PZEM_RX_PIN = 16;
constexpr uint8_t PZEM_TX_PIN = 17;
constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;

// ---- PZEM ----
constexpr uint8_t MAX_PHASES = 3;
constexpr uint8_t PZEM_ADDR[MAX_PHASES] = {0x01, 0x02, 0x03};
constexpr unsigned long PZEM_UART_BAUD = 9600;

// ---- Timing ----
constexpr unsigned long MQTT_RETRY_MS = 5000;
constexpr unsigned long GPIO_HOLD_MS = 5000;
constexpr unsigned long BOOT_AP_HOLD_MS = 3000;
constexpr uint8_t WDT_TIMEOUT_S = 10;
constexpr int WIFI_CONNECT_MAX_ATTEMPTS = 40;
constexpr unsigned long WIFI_CONNECT_RETRY_DELAY_MS = 500;
constexpr unsigned long WIFI_RECONNECT_RETRY_MS = 5000;

// ---- NTP ----
constexpr long NTP_GMT_OFFSET_SEC = -3 * 3600;
constexpr int NTP_DST_OFFSET_SEC = 0;
constexpr const char *NTP_SERVER_1 = "pool.ntp.org";
constexpr const char *NTP_SERVER_2 = "time.nist.gov";
constexpr int NTP_SYNC_MAX_ATTEMPTS = 20;
constexpr unsigned long NTP_SYNC_RETRY_DELAY_MS = 500;

// ---- Captive portal / AP ----
constexpr const char *AP_SSID = "ESP32_Config";

// ---- MQTT ----
#define DEFAULT_MQTT_TOPIC_PREFIX "vigiatech/setor/painel/CT"
