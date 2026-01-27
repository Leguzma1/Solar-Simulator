#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"
#include "esp_log.h"

typedef struct {
    spi_device_handle_t spi_handle;
    int pin_cs;
    struct {
        bool gainSelect; // true: 1x (Vout = Vref * D/4096), false: 2x (Vout = 2 * Vref * D/4096)
        bool active;     // true: active mode, false: shutdown
    } channel[2];        // channel[0]: output A, channel[1]: output B
} mcp4802_t;

esp_err_t mcp4802_init(mcp4802_t *dac);
esp_err_t mcp4802_write_reg(mcp4802_t *dac, uint8_t data, bool channel);
esp_err_t mcp4802_power_channel(mcp4802_t *dac, bool channel, bool power);