#include <stdlib.h>
#include "esp_log.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "switch.h"


static const uint32_t ALL_SD_PINS = GPIO_SEL_14|GPIO_SEL_15|GPIO_SEL_2|GPIO_SEL_4|GPIO_SEL_12|GPIO_SEL_13;
//static const uint32_t PIN_POWER = 23;

enum Mode {
    IDLE, FLASH, DEVICE
};
static enum Mode active_mode = IDLE;


bool enter_flash_mode(sdmmc_card_t *card) {
    ESP_LOGI("flash-mode", "Entering...");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    ESP_LOGI("flash-mode", "A.");

    /* example says external pullups should be necessary, but it seems to work without */
    sdmmc_host_pullup_en(SDMMC_HOST_SLOT_1, 4);
    ESP_LOGI("flash-mode", "B.");

    esp_err_t status = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
    if (status != ESP_OK) {
        ESP_LOGE("flash-mode", "Failed to initialize SD slot: %s", esp_err_to_name(status));
        abort();
    }
    ESP_LOGI("flash-mode", "C.");

    status = sdmmc_card_init(&host, card);
    if (status != ESP_OK) {
        ESP_LOGE("flash-mode",
                 "Failed to initialize SD card: %s. Make sure the card is inserted and the device is turned off.",
                 esp_err_to_name(status));
        return NULL;
    }

    active_mode = FLASH;
    ESP_LOGI("flash-mode", "Initialization complete.");

    return card;
}


void leave_flash_mode() {
    for (uint32_t pin = 0; pin < 32; pin++) {
        if (ALL_SD_PINS & (1u << pin)) {
            gpio_reset_pin(pin);
        }
    }
}
