#include "dac39j81q.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DAC_SPI_HOST    SPI2_HOST
// #define DAC_PIN_MISO    19
#define DAC_PIN_MOSI    7
#define DAC_PIN_SCLK    6
#define DAC_PIN_CS    16

static void spi_bus_init(void)
{
    static bool bus_inited = false;
    if (bus_inited) return;

    spi_bus_config_t buscfg = {
        // .miso_io_num = DAC_PIN_MISO,
        .mosi_io_num = DAC_PIN_MOSI,
        .sclk_io_num = DAC_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    spi_bus_initialize(DAC_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    bus_inited = true;
}

esp_err_t dac39j81q_init(dac39j81q_t *dac)
{
    spi_bus_init();

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,  // 10 MHz
        .mode = 1,                           // CPOL=0 CPHA=1 (verify datasheet)
        .spics_io_num = dac->pin_cs,
        .queue_size = 7,
    };

    return spi_bus_add_device(DAC_SPI_HOST, &devcfg, &dac->spi_handle);
}

esp_err_t dac39j81q_write_reg(dac39j81q_t *dac, uint16_t data)
{
    uint32_t full_value = (uint32_t)data;  // data: 0000 0011(high impedance mode) [value]. Total 24 bits, big endian.
    // uint32_t full_value = (3U << 16) | (uint32_t)data;  // data: 0000 0011(high impedance mode) [value]. Total 24 bits, big endian.
    uint8_t tx_buf[3] = {
        (full_value >> 16) & 0xFF,
        (full_value >>  8) & 0xFF,
        (full_value >>  0) & 0xFF
    };

    spi_transaction_t t = {
        .length = 24,      
        .tx_buffer = tx_buf,
        .rx_buffer = NULL
    };

    return spi_device_transmit(dac->spi_handle, &t);
}
