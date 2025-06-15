#include "ws2812.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <securec.h>
#include "pinctrl.h"
#include "gpio.h"
#include "soc_osal.h"
#include "spi.h"

void spi_gpio_init(void)
{
    uapi_pin_set_mode(SPI_DO_PIN, 3);
    uapi_pin_set_mode(SPI_CLK_PIN, 3);
    uapi_pin_set_mode(SPI_DI_PIN, 3);
}

void spi_init(void)
{
    spi_attr_t config = {0};
    spi_extra_attr_t ext_config = {0};

    config.is_slave = false;
    config.slave_num = SPI_SLAVE_NUM;
    config.bus_clk = 32000000;
    config.freq_mhz = SPI_FREQUENCY;
    config.clk_polarity = SPI_CLK_POLARITY;
    config.clk_phase = SPI_CLK_PHASE;
    config.frame_format = SPI_FRAME_FORMAT;
    config.spi_frame_format = HAL_SPI_FRAME_FORMAT_STANDARD;
    config.frame_size = HAL_SPI_FRAME_SIZE_8;
    config.tmod = SPI_TMOD;
    config.sste = 1;

    ext_config.qspi_param.wait_cycles = SPI_WAIT_CYCLES;
    ext_config.tx_use_dma = true;
    spi_gpio_init();
    uapi_spi_init(SPI_ID, &config, &ext_config);
}

void ws2812_send_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    spi_xfer_data_t data;
    uint8_t spi_data[24];

    // 转换G分量（高位在前）
    for (int i = 0; i < 8; i++)
    {
        spi_data[i] = (g & (1 << (7 - i))) ? 0xF8 : 0xC0;
    }

    // 转换R分量
    for (int i = 0; i < 8; i++)
    {
        spi_data[8 + i] = (r & (1 << (7 - i))) ? 0xF8 : 0xC0;
    }

    // 转换B分量
    for (int i = 0; i < 8; i++)
    {
        spi_data[16 + i] = (b & (1 << (7 - i))) ? 0xF8 : 0xC0;
    }

    data.tx_buff = spi_data;
    data.tx_bytes = 24;
    uapi_spi_master_write(SPI_ID, &data, 0xFFFFFFFF);
}

void ws2812_reset(void)
{
    uint8_t reset_data[300] = {0};
    spi_xfer_data_t data;
    data.tx_buff = reset_data;
    data.tx_bytes = 300;
    uapi_spi_master_write(SPI_ID, &data, 0xFFFFFFFF);
}

// HSV转RGB函数
void hsv2rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {
    int i;
    float f, p, q, t;
    
    if (s == 0) {
        *r = *g = *b = (uint8_t)(v * 255);
        return;
    }
    
    h /= 60;
    i = (int)h;
    f = h - i;
    p = v * (1 - s);
    q = v * (1 - s * f);
    t = v * (1 - s * (1 - f));
    
    switch (i) {
        case 0: *r = (uint8_t)(v * 255); *g = (uint8_t)(t * 255); *b = (uint8_t)(p * 255); break;
        case 1: *r = (uint8_t)(q * 255); *g = (uint8_t)(v * 255); *b = (uint8_t)(p * 255); break;
        case 2: *r = (uint8_t)(p * 255); *g = (uint8_t)(v * 255); *b = (uint8_t)(t * 255); break;
        case 3: *r = (uint8_t)(p * 255); *g = (uint8_t)(q * 255); *b = (uint8_t)(v * 255); break;
        case 4: *r = (uint8_t)(t * 255); *g = (uint8_t)(p * 255); *b = (uint8_t)(v * 255); break;
        default: *r = (uint8_t)(v * 255); *g = (uint8_t)(p * 255); *b = (uint8_t)(q * 255); break;
    }
}
