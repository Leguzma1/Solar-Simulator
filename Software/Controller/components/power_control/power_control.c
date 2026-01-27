#include "power_control.h"

static const char *TAG = "power_control";

static mcp4802_t dac = {
    .pin_cs = 16,  // your CS pin
    .channel[0] = { // channel A
        .gainSelect = false, // 2x gain
        .active = true, // active mode
    },
    .channel[1] = { // channel B
        .gainSelect = false, // 2x gain
        .active = true, // active mode
    }
};

led_t white_led= {
    .channel = WHITE_CHANNEL,
    .state_str = "off",
    .power = LED_FLAG_OFF,
    .dac = &dac
};

void power_init(void)
{
    mcp4802_init(&dac);
    // Initialize all LEDs to off
    led_power_control(&white_led, LED_FLAG_OFF);
}

void led_power_control(led_t *led, float power_level)
{
    if (power_level < 0.0f) {
        power_level = 0.0f;
    } else if (power_level > 1.0f) {
        power_level = 1.0f;
    }
    led->power = power_level;

    uint8_t dac_value = (uint8_t)(power_level * 255.0f);
    ESP_LOGI(TAG, "Setting LED channel %d to power level %.2f (DAC value: %d)", 
             led->channel, led->power, dac_value);

    mcp4802_write_reg(led->dac, dac_value, led->channel);
}
