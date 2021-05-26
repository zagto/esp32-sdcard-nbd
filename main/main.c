#include "driver/sdmmc_host.h"
#include "wifi.h"
#include "server.h"
#include "host.h"
#include "switch.h"
#include <pthread.h>

static void *server_thread_main(void *unused) {
    server_task();
}

static void *host_thread_main(void *ununsed) {
    host_task();
}

void app_main(void) {
    sdmmc_host_init();
    initialize_switch();
    initialize_wifi();
    pthread_t host_thread;
    pthread_create(&host_thread, NULL, host_thread_main, NULL);
    server_task();
    //host_task();
}
