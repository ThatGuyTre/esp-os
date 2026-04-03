#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_lcd_panel_io.h"

typedef struct {
    bool bgr;
    bool invert;
    uint8_t madctl_base;
    uint16_t col_start;
    uint16_t row_start;
} st7735_panel_config_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} screen_rect_t;

void st7735_init_panel(esp_lcd_panel_io_handle_t io, const st7735_panel_config_t *cfg);
void st7735_set_window(esp_lcd_panel_io_handle_t io, const st7735_panel_config_t *cfg,
                       uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);

screen_rect_t screen_rect_make(int x, int y, int w, int h);
screen_rect_t screen_rect_inflate(screen_rect_t rect, int amount, int max_w, int max_h);
screen_rect_t screen_rect_union(screen_rect_t a, screen_rect_t b);

void screen_fill(uint16_t *fb, uint16_t width, uint16_t height, uint16_t color);
void screen_fill_rect(uint16_t *fb, uint16_t width, uint16_t height, screen_rect_t rect, uint16_t color);
void screen_push(esp_lcd_panel_io_handle_t io, const st7735_panel_config_t *cfg,
                 const uint16_t *fb, uint16_t width, uint16_t height);
void screen_push_rect(esp_lcd_panel_io_handle_t io, const st7735_panel_config_t *cfg,
                      const uint16_t *fb, uint16_t width, uint16_t height, screen_rect_t rect);
void screen_draw_pixel(uint16_t *fb, uint16_t width, uint16_t height,
                       int x, int y, uint16_t color);
void screen_draw_line(uint16_t *fb, uint16_t width, uint16_t height,
                      int x0, int y0, int x1, int y1, uint16_t color);
void screen_draw_box(uint16_t *fb, uint16_t width, uint16_t height,
                     int x, int y, int w, int h, uint16_t color, bool filled);
void screen_draw_ellipse(uint16_t *fb, uint16_t width, uint16_t height,
                         int cx, int cy, int rx, int ry, uint16_t color, bool filled);
void screen_draw_smiley(uint16_t *fb, uint16_t width, uint16_t height,
                        int cx, int cy, int radius, uint16_t color);
