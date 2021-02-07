#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/sdmmc_types.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"

#include "wifi.h"
#include "server.h"



/*
static void enter_device_mode(void) {
    gpio_reset_pin(PIN_POWER);
    for (uint32_t pin = 0; pin < 32; pin++) {
        if (ALL_SD_PINS & (1u << pin)) {
            gpio_reset_pin(pin);
        }
    }
    gpio_config_t check_config = {
        .pin_bit_mask = 1u<<PIN_POWER,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&check_config);
}

static void enter_esp_mode(void) {

    gpio_config_t write_config = {
        .pin_bit_mask = 1u << PIN_POWER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
   gpio_config(&write_config);
   gpio_set_drive_capability(PIN_POWER, 3);
   gpio_set_level(PIN_POWER, 1);
}
*/


//#include "switch.h"
void app_main(void) {
    sdmmc_host_init();
    initialize_wifi("FRITZ!Box 7412", "unse​aled​ sph​ere");
    server_task();

    //static char buffer[2048];

    //sdmmc_card_t card;
    //enter_flash_mode(&card);
    //ESP_LOGI("test", "read result: %u", sdmmc_read_sectors(&card, buffer, 0, 1));
}

