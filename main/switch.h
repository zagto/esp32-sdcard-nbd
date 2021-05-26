#pragma once

#include <stdbool.h>
#include "driver/sdmmc_host.h"

bool enter_flash_mode(sdmmc_card_t *card);
void leave_flash_mode(void);
bool enter_host_mode(void);
void leave_host_mode(void);
void initialize_switch(void);
