#pragma once

#include "esp_log.h"
#include "MCP4802.h"

#define LED_FLAG_ON 1
#define LED_FLAG_OFF 0

/* Define channel numbers */
#define WHITE_CHANNEL 0

typedef struct {
    uint8_t channel;            // channel identifier
    const char *state_str;      // description of the state
    float     power;            // power float: 0.0 to 1.0
    mcp4802_t *dac;              // DAC instance
} led_t;

extern led_t white_led; 

void led_power_control(led_t *led, float power_level);
void power_init(void);


