/*
 * bsp.c -- 板级实现：GPIO + SPI 总线 + 背光
 */
#include "bsp.h"

#include "board_pins.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bsp";

static void backlight_pwm_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .gpio_num = BOARD_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0, /* BLK active-high: 0% duty = backlight off */
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

esp_err_t bsp_init(void)
{
    /* DC / RST 两个普通输出脚；BL 高电平点亮，走 LEDC PWM */
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << BOARD_LCD_DC) | (1ULL << BOARD_LCD_RST),
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
    backlight_pwm_init();
    bsp_backlight_set(0);
    gpio_set_level(BOARD_LCD_RST, 1);
    gpio_set_level(BOARD_LCD_DC, 1);

    /* SPI2 总线：只写，无 MISO。
     * 必须开 DMA：RGB565 一行 240*2 = 480 字节。 */
    spi_bus_config_t bus = {
        .mosi_io_num = BOARD_LCD_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BOARD_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = BOARD_LCD_WIDTH * 2,    /* RGB565 一行 240*2 = 480 字节 */
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
    bsp_backlight_set(on ? 100 : 0);
}

void bsp_backlight_set(uint8_t percent)
{
    if (percent > 100) {
        percent = 100;
    }
    /* 模块 BLK 低电平关闭、高电平点亮。 */
    const uint32_t duty = (uint32_t)percent * 1023U / 100U;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
