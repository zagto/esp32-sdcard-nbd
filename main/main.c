#include "driver/sdmmc_host.h"
#include "wifi.h"
#include "server.h"
#include "config.h"


void app_main(void) {
    sdmmc_host_init();
    initialize_wifi(WIFI_SSID, WIFI_PASSWORD);
    server_task();
}
