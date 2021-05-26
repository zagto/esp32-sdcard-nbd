#include "driver/gpio.h"
#include "host.h"
#include "switch.h"
#include "pins.h"
#include "freertos/task.h"
#include "esp_log.h"


void host_task(void) {
    while (true) {
        vTaskDelay(25);

        while (gpio_get_level(HOST_VOLT_DETECT) == 0) {
            vTaskDelay(25);
        }
        ESP_LOGI("host", "Detected host power up");
        if (!enter_host_mode()) {
            ESP_LOGE("host", "Could not enter host mode - Please disconnect NBD client!");
            vTaskDelay(1000);
            continue;
        }

        while (gpio_get_level(HOST_VOLT_DETECT) == 1) {
            vTaskDelay(25);
        }
        ESP_LOGI("host", "Detected host power down");
        leave_host_mode();
    }
}
