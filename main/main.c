#include "driver/sdmmc_host.h"
#include "wifi.h"
#include "server.h"


void app_main(void) {
    sdmmc_host_init();
    initialize_wifi();
    server_task();
}
