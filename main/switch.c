#include <stdlib.h>
#include <pthread.h>
#include "esp_log.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "switch.h"
#include "pins.h"
#include "freertos/task.h"


static const uint64_t ALL_SD_PINS = GPIO_SEL_14|GPIO_SEL_15|GPIO_SEL_2|GPIO_SEL_4|GPIO_SEL_12|GPIO_SEL_13;

enum Mode {
    IDLE, FLASH, DEVICE
};
static enum Mode active_mode = IDLE;


static pthread_mutex_t access_mutex = PTHREAD_MUTEX_INITIALIZER;


bool enter_flash_mode(sdmmc_card_t *card) {
    if (pthread_mutex_trylock(&access_mutex) != 0) {
        return false;
    }
    ESP_LOGI("flash-mode", "Entering...");

    gpio_set_level(SD_POWER_ESP, 0);
    gpio_set_level(BUS_SW_ESP, 1);

    vTaskDelay(25);

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    /* example says external pullups should be necessary, but it seems to work without */
    sdmmc_host_pullup_en(SDMMC_HOST_SLOT_1, 4);

    esp_err_t status = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
    if (status != ESP_OK) {
        ESP_LOGE("flash-mode", "Failed to initialize SD slot: %s", esp_err_to_name(status));
        abort();
    }

    status = sdmmc_card_init(&host, card);
    if (status != ESP_OK) {
        ESP_LOGE("flash-mode",
                 "Failed to initialize SD card: %s. Make sure the card is inserted and the device is turned off.",
                 esp_err_to_name(status));
        return false;
    }

    active_mode = FLASH;
    ESP_LOGI("flash-mode", "Initialization complete.");

    return true;
}

void leave_flash_mode(void) {
    ESP_LOGI("flash-mode", "Leaving...");
    gpio_set_level(BUS_SW_ESP, 0);
    for (uint64_t pin = 0; pin < 64; pin++) {
        if (ALL_SD_PINS & (1ull << pin)) {
            gpio_reset_pin(pin);
        }
    }
    vTaskDelay(25);
    gpio_set_level(SD_POWER_ESP, 1);

    vTaskDelay(25);
    pthread_mutex_unlock(&access_mutex);
}

bool enter_host_mode() {
    if (pthread_mutex_trylock(&access_mutex) != 0) {
        return false;
    }
    ESP_LOGI("host-mode", "Entering...");

    gpio_set_level(SD_POWER_HOST, 0);
    gpio_set_level(BUS_SW_HOST, 1);
    return true;
}

void leave_host_mode() {
    ESP_LOGI("host-mode", "Leaving...");
    gpio_set_level(BUS_SW_HOST, 0);
    vTaskDelay(25);
    gpio_set_level(SD_POWER_HOST, 1);
    vTaskDelay(25);
    pthread_mutex_unlock(&access_mutex);
}

void initialize_switch(void) {
    gpio_config_t power_config = {
        (1ull<<SD_POWER_HOST)|(1ull<<SD_POWER_ESP)|(1ull<<BUS_SW_ESP)|(1ull<<BUS_SW_HOST),
        GPIO_MODE_OUTPUT,
        GPIO_PULLUP_DISABLE,
        GPIO_PULLDOWN_DISABLE,
        GPIO_INTR_DISABLE
    };

    gpio_config(&power_config);

    gpio_config_t input_config = {
        (1ull<<HOST_VOLT_DETECT),
        GPIO_MODE_INPUT,
        GPIO_PULLUP_DISABLE,
        GPIO_PULLDOWN_ENABLE,
        GPIO_INTR_DISABLE
    };
    gpio_config(&input_config);

   // leave_flash_mode();

    gpio_set_level(SD_POWER_HOST, 1);
    gpio_set_level(BUS_SW_HOST, 0);
    gpio_set_level(SD_POWER_ESP, 1);
    gpio_set_level(BUS_SW_ESP, 0);

    vTaskDelay(25);
}
