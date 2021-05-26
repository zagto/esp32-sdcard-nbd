#pragma once

#define LED_GN 25
#define LED_RT 26

#define BUS_SW_HOST 16

#define BUS_SW_ESP 17

// for Power over MOSFET

#define POWER_OVER_MOSFET 0

#define SD_MOSFET_POWER_SWITCH 32
#define SD_POWER_HOST 33
#define SD_POWER_ESP 27


// SD Interface
#define ESP_CLK 14
#define ESP_CMD 15
#define ESP_D0   2
#define ESP_D1   4
#define ESP_D2  12
#define ESP_D3  13

// SPI
#define ESP_MOSI 23
#define ESP_MISO 19
#define ESP_SCLK 18
#define ESP_CS    5
#define SD_CS ESP_CS

// I2C
#define ESP_SDA 21
#define ESP_SCL 22

#define SD_DETECT_PIN 35

#define HOST_VOLT_DETECT 39
