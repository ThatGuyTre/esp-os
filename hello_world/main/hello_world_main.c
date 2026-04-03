/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "screen.h"

// ST7735 wiring (update these pin numbers for your board).
#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_NUM_DC   17
#define PIN_NUM_RES  16
#define PIN_NUM_BL   4

// 1.8" ST7735 resolution.
#define LCD_H_RES 128
#define LCD_V_RES 160

// Backlight polarity and panel options.
#define BL_ON_LEVEL 1
#define PANEL_BGR 0
#define PANEL_INVERT 0
#define PANEL_COL_START 0
#define PANEL_ROW_START 0
#define PANEL_MADCTL_BASE 0xC0

// SPI configuration that worked for this panel.
#define LCD_SPI_MODE 0
#define LCD_PCLK_HZ (20 * 1000 * 1000)

// Full-screen RGB565 framebuffer in static storage (not on task stack).
static uint16_t s_frame_buffer[LCD_H_RES * LCD_V_RES];

void app_main(void)
{
    // Configure and initialize the SPI bus used by the LCD.
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Configure the command/data panel IO layer on top of SPI.
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = LCD_PCLK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = LCD_SPI_MODE,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    // Hardware reset pulse for the LCD controller.
    gpio_set_direction(PIN_NUM_RES, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_RES, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_NUM_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Panel profile corresponds to the previous hard-coded constants.
    const st7735_panel_config_t panel_cfg = {
        .bgr = PANEL_BGR,
        .invert = PANEL_INVERT,
        .madctl_base = PANEL_MADCTL_BASE,
        .col_start = PANEL_COL_START,
        .row_start = PANEL_ROW_START,
    };

    // Initialize controller registers and turn the display on.
    st7735_init_panel(io_handle, &panel_cfg);

    // Enable LCD backlight.
    gpio_set_direction(PIN_NUM_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_BL, BL_ON_LEVEL);

    const uint16_t background_color = 0x0000;
    const uint16_t smiley_color = 0xFFFF;
    const int radius = 24;

    int x = LCD_H_RES / 2;
    int y = LCD_V_RES / 2;
    int vx = 2;
    int vy = 2;

    screen_fill(s_frame_buffer, LCD_H_RES, LCD_V_RES, background_color);
    screen_draw_smiley(s_frame_buffer, LCD_H_RES, LCD_V_RES, x, y, radius, smiley_color);
    screen_push(io_handle, &panel_cfg, s_frame_buffer, LCD_H_RES, LCD_V_RES);

    screen_rect_t prev_rect = screen_rect_make(x - radius - 2, y - radius - 2, (radius + 2) * 2, (radius + 2) * 2);

    while (true) {
        int next_x = x + vx;
        int next_y = y + vy;

        if (next_x - radius <= 0 || next_x + radius >= LCD_H_RES - 1) {
            vx = -vx;
            next_x = x + vx;
        }
        if (next_y - radius <= 0 || next_y + radius >= LCD_V_RES - 1) {
            vy = -vy;
            next_y = y + vy;
        }

        screen_rect_t next_rect = screen_rect_make(next_x - radius - 2, next_y - radius - 2, (radius + 2) * 2, (radius + 2) * 2);
        screen_rect_t dirty = screen_rect_union(prev_rect, next_rect);
        dirty = screen_rect_inflate(dirty, 1, LCD_H_RES, LCD_V_RES);

        screen_fill_rect(s_frame_buffer, LCD_H_RES, LCD_V_RES, dirty, background_color);
        screen_draw_smiley(s_frame_buffer, LCD_H_RES, LCD_V_RES, next_x, next_y, radius, smiley_color);
        screen_push_rect(io_handle, &panel_cfg, s_frame_buffer, LCD_H_RES, LCD_V_RES, dirty);

        x = next_x;
        y = next_y;
        prev_rect = next_rect;

        vTaskDelay(pdMS_TO_TICKS(25));
    }
}
