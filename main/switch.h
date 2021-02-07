#pragma once

#include <stdbool.h>
#include "driver/sdmmc_host.h"

bool enter_flash_mode(sdmmc_card_t *card);
void leave_flash_mode();
