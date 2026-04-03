#include "screen.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define SCREEN_PUSH_MAX_PIXELS (128 * 160)

#define ST7735_CMD_SWRESET 0x01
#define ST7735_CMD_SLPOUT  0x11
#define ST7735_CMD_NORON   0x13
#define ST7735_CMD_FRMCTR1 0xB1
#define ST7735_CMD_FRMCTR2 0xB2
#define ST7735_CMD_FRMCTR3 0xB3
#define ST7735_CMD_INVCTR  0xB4
#define ST7735_CMD_PWCTR1  0xC0
#define ST7735_CMD_PWCTR2  0xC1
#define ST7735_CMD_PWCTR3  0xC2
#define ST7735_CMD_PWCTR4  0xC3
#define ST7735_CMD_PWCTR5  0xC4
#define ST7735_CMD_VMCTR1  0xC5
#define ST7735_CMD_INVOFF  0x20
#define ST7735_CMD_INVON   0x21
#define ST7735_CMD_DISPON  0x29
#define ST7735_CMD_CASET   0x2A
#define ST7735_CMD_RASET   0x2B
#define ST7735_CMD_RAMWR   0x2C
#define ST7735_CMD_MADCTL  0x36
#define ST7735_CMD_COLMOD  0x3A
#define ST7735_CMD_GMCTRP1 0xE0
#define ST7735_CMD_GMCTRN1 0xE1

static void st7735_cmd(esp_lcd_panel_io_handle_t io, uint8_t cmd)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, cmd, NULL, 0));
}

static void st7735_data(esp_lcd_panel_io_handle_t io, uint8_t cmd, const void *data, size_t len)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io, cmd, data, len));
}

static uint16_t s_push_buffer[SCREEN_PUSH_MAX_PIXELS];

screen_rect_t screen_rect_make(int x, int y, int w, int h)
{
    screen_rect_t rect = { x, y, w, h };
    return rect;
}

screen_rect_t screen_rect_inflate(screen_rect_t rect, int amount, int max_w, int max_h)
{
    rect.x -= amount;
    rect.y -= amount;
    rect.w += amount * 2;
    rect.h += amount * 2;

    if (rect.x < 0) {
        rect.w += rect.x;
        rect.x = 0;
    }
    if (rect.y < 0) {
        rect.h += rect.y;
        rect.y = 0;
    }
    if (rect.x + rect.w > max_w) {
        rect.w = max_w - rect.x;
    }
    if (rect.y + rect.h > max_h) {
        rect.h = max_h - rect.y;
    }

    if (rect.w < 0) {
        rect.w = 0;
    }
    if (rect.h < 0) {
        rect.h = 0;
    }
    return rect;
}

screen_rect_t screen_rect_union(screen_rect_t a, screen_rect_t b)
{
    if (a.w <= 0 || a.h <= 0) {
        return b;
    }
    if (b.w <= 0 || b.h <= 0) {
        return a;
    }

    int x0 = (a.x < b.x) ? a.x : b.x;
    int y0 = (a.y < b.y) ? a.y : b.y;
    int x1 = ((a.x + a.w) > (b.x + b.w)) ? (a.x + a.w) : (b.x + b.w);
    int y1 = ((a.y + a.h) > (b.y + b.h)) ? (a.y + a.h) : (b.y + b.h);
    return screen_rect_make(x0, y0, x1 - x0, y1 - y0);
}

void st7735_init_panel(esp_lcd_panel_io_handle_t io, const st7735_panel_config_t *cfg)
{
    static const uint8_t frmctr1[] = {0x01, 0x2C, 0x2D};
    static const uint8_t frmctr2[] = {0x01, 0x2C, 0x2D};
    static const uint8_t frmctr3[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    static const uint8_t invctr[] = {0x07};
    static const uint8_t pwctr1[] = {0xA2, 0x02, 0x84};
    static const uint8_t pwctr2[] = {0xC5};
    static const uint8_t pwctr3[] = {0x0A, 0x00};
    static const uint8_t pwctr4[] = {0x8A, 0x2A};
    static const uint8_t pwctr5[] = {0x8A, 0xEE};
    static const uint8_t vmctr1[] = {0x0E};
    static const uint8_t gamma_pos[] = {
        0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
        0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
    };
    static const uint8_t gamma_neg[] = {
        0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
        0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
    };

    st7735_cmd(io, ST7735_CMD_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    st7735_cmd(io, ST7735_CMD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    st7735_data(io, ST7735_CMD_FRMCTR1, frmctr1, sizeof(frmctr1));
    st7735_data(io, ST7735_CMD_FRMCTR2, frmctr2, sizeof(frmctr2));
    st7735_data(io, ST7735_CMD_FRMCTR3, frmctr3, sizeof(frmctr3));
    st7735_data(io, ST7735_CMD_INVCTR, invctr, sizeof(invctr));
    st7735_data(io, ST7735_CMD_PWCTR1, pwctr1, sizeof(pwctr1));
    st7735_data(io, ST7735_CMD_PWCTR2, pwctr2, sizeof(pwctr2));
    st7735_data(io, ST7735_CMD_PWCTR3, pwctr3, sizeof(pwctr3));
    st7735_data(io, ST7735_CMD_PWCTR4, pwctr4, sizeof(pwctr4));
    st7735_data(io, ST7735_CMD_PWCTR5, pwctr5, sizeof(pwctr5));
    st7735_data(io, ST7735_CMD_VMCTR1, vmctr1, sizeof(vmctr1));

    uint8_t colmod = 0x05; // RGB565
    st7735_data(io, ST7735_CMD_COLMOD, &colmod, 1);

    uint8_t madctl = (uint8_t)(cfg->madctl_base | (cfg->bgr ? 0x08 : 0x00));
    st7735_data(io, ST7735_CMD_MADCTL, &madctl, 1);

    if (cfg->invert) {
        st7735_cmd(io, ST7735_CMD_INVON);
    } else {
        st7735_cmd(io, ST7735_CMD_INVOFF);
    }

    st7735_data(io, ST7735_CMD_GMCTRP1, gamma_pos, sizeof(gamma_pos));
    st7735_data(io, ST7735_CMD_GMCTRN1, gamma_neg, sizeof(gamma_neg));

    st7735_cmd(io, ST7735_CMD_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));
    st7735_cmd(io, ST7735_CMD_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));
}

void st7735_set_window(esp_lcd_panel_io_handle_t io, const st7735_panel_config_t *cfg,
                       uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
    uint16_t x0 = xs + cfg->col_start;
    uint16_t x1 = xe + cfg->col_start;
    uint16_t y0 = ys + cfg->row_start;
    uint16_t y1 = ye + cfg->row_start;

    uint8_t caset[4] = {
        (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF),
    };
    uint8_t raset[4] = {
        (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF),
    };

    st7735_data(io, ST7735_CMD_CASET, caset, sizeof(caset));
    st7735_data(io, ST7735_CMD_RASET, raset, sizeof(raset));
}

void screen_fill(uint16_t *fb, uint16_t width, uint16_t height, uint16_t color)
{
    for (int i = 0; i < width * height; i++) {
        fb[i] = color;
    }
}

void screen_fill_rect(uint16_t *fb, uint16_t width, uint16_t height, screen_rect_t rect, uint16_t color)
{
    int x0 = rect.x;
    int y0 = rect.y;
    int x1 = rect.x + rect.w;
    int y1 = rect.y + rect.h;

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > (int)width) {
        x1 = width;
    }
    if (y1 > (int)height) {
        y1 = height;
    }

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            fb[y * width + x] = color;
        }
    }
}

void screen_push(esp_lcd_panel_io_handle_t io, const st7735_panel_config_t *cfg,
                 const uint16_t *fb, uint16_t width, uint16_t height)
{
    st7735_set_window(io, cfg, 0, 0, width - 1, height - 1);
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(io, ST7735_CMD_RAMWR, fb, width * height * sizeof(uint16_t)));
}

void screen_push_rect(esp_lcd_panel_io_handle_t io, const st7735_panel_config_t *cfg,
                      const uint16_t *fb, uint16_t width, uint16_t height, screen_rect_t rect)
{
    int x0 = rect.x;
    int y0 = rect.y;
    int x1 = rect.x + rect.w - 1;
    int y1 = rect.y + rect.h - 1;

    if (rect.w <= 0 || rect.h <= 0) {
        return;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 >= (int)width) {
        x1 = width - 1;
    }
    if (y1 >= (int)height) {
        y1 = height - 1;
    }

    st7735_set_window(io, cfg, (uint16_t)x0, (uint16_t)y0, (uint16_t)x1, (uint16_t)y1);

    int region_w = x1 - x0 + 1;
    int region_h = y1 - y0 + 1;
    int region_pixels = region_w * region_h;

    if (region_pixels > SCREEN_PUSH_MAX_PIXELS) {
        screen_push(io, cfg, fb, width, height);
        return;
    }

    for (int row = 0; row < region_h; row++) {
        memcpy(&s_push_buffer[row * region_w], &fb[(y0 + row) * width + x0], (size_t)region_w * sizeof(uint16_t));
    }

    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_color(io, ST7735_CMD_RAMWR, s_push_buffer, region_pixels * sizeof(uint16_t)));
}

void screen_draw_pixel(uint16_t *fb, uint16_t width, uint16_t height,
                       int x, int y, uint16_t color)
{
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    fb[y * width + x] = color;
}

void screen_draw_line(uint16_t *fb, uint16_t width, uint16_t height,
                      int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        screen_draw_pixel(fb, width, height, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void screen_draw_box(uint16_t *fb, uint16_t width, uint16_t height,
                     int x, int y, int w, int h, uint16_t color, bool filled)
{
    if (w <= 0 || h <= 0) {
        return;
    }

    if (filled) {
        for (int yy = y; yy < y + h; yy++) {
            for (int xx = x; xx < x + w; xx++) {
                screen_draw_pixel(fb, width, height, xx, yy, color);
            }
        }
    } else {
        screen_draw_line(fb, width, height, x, y, x + w - 1, y, color);
        screen_draw_line(fb, width, height, x, y + h - 1, x + w - 1, y + h - 1, color);
        screen_draw_line(fb, width, height, x, y, x, y + h - 1, color);
        screen_draw_line(fb, width, height, x + w - 1, y, x + w - 1, y + h - 1, color);
    }
}

void screen_draw_ellipse(uint16_t *fb, uint16_t width, uint16_t height,
                         int cx, int cy, int rx, int ry, uint16_t color, bool filled)
{
    if (rx <= 0 || ry <= 0) {
        return;
    }

    for (int y = -ry; y <= ry; y++) {
        for (int x = -rx; x <= rx; x++) {
            long lhs = (long)x * x * (long)ry * ry + (long)y * y * (long)rx * rx;
            long rhs = (long)rx * rx * (long)ry * ry;

            if (filled) {
                if (lhs <= rhs) {
                    screen_draw_pixel(fb, width, height, cx + x, cy + y, color);
                }
            } else {
                long band = rhs / ((rx + ry) > 0 ? (rx + ry) : 1);
                if (lhs >= (rhs - band) && lhs <= (rhs + band)) {
                    screen_draw_pixel(fb, width, height, cx + x, cy + y, color);
                }
            }
        }
    }
}

void screen_draw_smiley(uint16_t *fb, uint16_t width, uint16_t height,
                        int cx, int cy, int radius, uint16_t color)
{
    int eye_offset_x = radius / 3;
    int eye_offset_y = radius / 4;
    int eye_r = (radius / 8) > 1 ? (radius / 8) : 1;

    // Face outline
    screen_draw_ellipse(fb, width, height, cx, cy, radius, radius, color, false);

    // Eyes
    screen_draw_ellipse(fb, width, height, cx - eye_offset_x, cy - eye_offset_y, eye_r, eye_r, color, true);
    screen_draw_ellipse(fb, width, height, cx + eye_offset_x, cy - eye_offset_y, eye_r, eye_r, color, true);

    // Smile arc (lower half of an ellipse)
    int smile_rx = radius / 2;
    int smile_ry = radius / 3;
    int smile_cy = cy + radius / 6;
    for (int y = 0; y <= smile_ry; y++) {
        for (int x = -smile_rx; x <= smile_rx; x++) {
            long lhs = (long)x * x * (long)smile_ry * smile_ry + (long)y * y * (long)smile_rx * smile_rx;
            long rhs = (long)smile_rx * smile_rx * (long)smile_ry * smile_ry;
            long band = rhs / ((smile_rx + smile_ry) > 0 ? (smile_rx + smile_ry) : 1);
            if (lhs >= (rhs - band) && lhs <= (rhs + band)) {
                screen_draw_pixel(fb, width, height, cx + x, smile_cy + y, color);
            }
        }
    }
}
