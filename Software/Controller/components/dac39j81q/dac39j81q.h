#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"

typedef struct {
    spi_device_handle_t spi_handle;
    int pin_cs;
} dac39j81q_t;

esp_err_t dac39j81q_init(dac39j81q_t *dac);
esp_err_t dac39j81q_write_reg(dac39j81q_t *dac, uint16_t data);
