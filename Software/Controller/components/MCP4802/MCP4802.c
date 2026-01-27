#include "MCP4802.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mcp4802";

#define DAC_SPI_HOST    SPI2_HOST
#define DAC_PIN_MOSI    7
#define DAC_PIN_SCLK    6

static void spi_bus_init(void)
{
    static bool bus_inited = false;
    if (bus_inited) return;

    spi_bus_config_t buscfg = {
        .mosi_io_num = DAC_PIN_MOSI,
        .sclk_io_num = DAC_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    spi_bus_initialize(DAC_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    bus_inited = true;
}

esp_err_t mcp4802_init(mcp4802_t *dac)
{
    spi_bus_init();

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 20 * 1000 * 1000,  // 20 MHz
        .mode = 0,                           // CPOL=0 CPHA=0 (verify datasheet)
        .spics_io_num = dac->pin_cs,
        .queue_size = 7,
    };

    return spi_bus_add_device(DAC_SPI_HOST, &devcfg, &dac->spi_handle);
}

esp_err_t mcp4802_write_reg(mcp4802_t *dac, uint8_t data, bool channel)
{
    uint8_t tx_buf[2] = {
        ((channel << 7) | (0 << 6) | (dac->channel[channel].gainSelect << 5) | (dac->channel[channel].active << 4) | ((data >> 4) & 0x0F)),
        ((data << 4) & 0xFF)
    };

    spi_transaction_t t = {
        .length = 16,      
        .tx_buffer = tx_buf,
        .rx_buffer = NULL
    };
    ESP_LOGI(TAG, "Sending SPI transaction:%02X %02X\n", tx_buf[0], tx_buf[1]);

    return spi_device_transmit(dac->spi_handle, &t);
}

// power = true: on, false: off
esp_err_t mcp4802_power_channel(mcp4802_t *dac, bool channel, bool power) 
{
    dac->channel[channel].active = power;
    return mcp4802_write_reg(dac, 0, channel);
}
