/*
 * bsp.c -- 板级实现：GPIO + SPI 总线 + 背光
 */
#include "bsp.h"

#include "board_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp";

esp_err_t bsp_init(void)
{
    /* DC / RST / BL 三个普通输出脚 */
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << BOARD_LCD_DC) | (1ULL << BOARD_LCD_RST) | (1ULL << BOARD_LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&out);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO 配置失败: %s", esp_err_to_name(err));
        return err;
    }
    gpio_set_level(BOARD_LCD_BL, 0);   /* 先关背光，等第一帧画完再开 */
    gpio_set_level(BOARD_LCD_RST, 1);
    gpio_set_level(BOARD_LCD_DC, 1);

    /* SPI2 总线：只写，无 MISO。
     * 必须开 DMA：ESP-IDF 在 CPU 模式下单次传输上限只有 64 字节，
     * 而横屏一行要发 160*3 = 480 字节，否则传输会被驱动拒绝、屏幕花屏。
     * max_transfer_sz 按较长边（160）计算。 */
    spi_bus_config_t bus = {
        .mosi_io_num = BOARD_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BOARD_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_HEIGHT * 3,   /* 横屏一行 160*3 = 480 字节 */
    };
    err = spi_bus_initialize(BOARD_LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI 总线初始化失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "就绪: SCLK=%d MOSI=%d CS=%d DC=%d RST=%d BL=%d, SPI %d Hz",
             BOARD_LCD_SCLK, BOARD_LCD_MOSI, BOARD_LCD_CS, BOARD_LCD_DC,
             BOARD_LCD_RST, BOARD_LCD_BL, BOARD_LCD_SPI_HZ);
    return ESP_OK;
}

esp_err_t bsp_lcd_add_device(spi_device_handle_t *out_dev)
{
    if (out_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    spi_device_interface_config_t dev = {
        .clock_speed_hz = BOARD_LCD_SPI_HZ,
        .mode = 0,                 /* CPOL=0, CPHA=0 */
        .spics_io_num = BOARD_LCD_CS,
        .queue_size = 1,
    };
    return spi_bus_add_device(BOARD_LCD_SPI_HOST, &dev, out_dev);
}

void bsp_lcd_dc(bool data)
{
    gpio_set_level(BOARD_LCD_DC, data ? 1 : 0);
}

void bsp_lcd_reset(void)
{
    gpio_set_level(BOARD_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(BOARD_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

void bsp_backlight(bool on)
{
    gpio_set_level(BOARD_LCD_BL, on ? 1 : 0);
}
