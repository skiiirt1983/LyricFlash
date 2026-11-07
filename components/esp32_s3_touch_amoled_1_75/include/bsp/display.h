

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_lcd_types.h"
#include "driver/gpio.h"

/* LCD color formats */
#define ESP_LCD_COLOR_FORMAT_RGB565    (1)
#define ESP_LCD_COLOR_FORMAT_RGB888    (2)

/* LCD display color format */
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
/* LCD display color bytes endianess */
#define BSP_LCD_BIGENDIAN           (0)
/* LCD display color bits */
#define BSP_LCD_BITS_PER_PIXEL      (16)
/* LCD display color space */
#define BSP_LCD_COLOR_SPACE         (ESP_LCD_COLOR_SPACE_RGB)
/* LCD display definition */
#define BSP_LCD_H_RES              (466)
#define BSP_LCD_V_RES              (466)
#define BSP_LCD_TE_GPIO            (GPIO_NUM_13)  // TE signal: CO5300 → ESP32-S3 GPIO13

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP display configuration structure
 *
 */
typedef struct {
    int max_transfer_sz;    /*!< Maximum transfer size, in bytes. */
} bsp_display_config_t;

/* Lock-free, read-only transfer measurements for on-device diagnostics. */
typedef struct {
    uint32_t stage_rows;             /* 8-row normal, 4-row fallback, 0 before first flush */
    uint32_t chunk_submissions;      /* physical QSPI bitmap submissions since boot */
    uint32_t stage_call_last_us;     /* time to submit one LVGL logical flush */
    uint32_t stage_call_max_us;
    uint32_t stage_cumulative_us; /* per-render snapshots subtract modulo uint32 */
} bsp_display_transfer_stats_t;
void bsp_display_transfer_stats(bsp_display_transfer_stats_t *out);

/* 0.8.113: boot-time panel facts for on-device diagnostics. */
typedef struct {
    bool lvgl_buffers_internal;   /* true: double 24-row SRAM strips (fast path) */
    uint32_t lvgl_buffer_rows;
    uint32_t internal_free_at_init;
    uint32_t selftest_kbytes_per_s; /* measured full-frame QSPI throughput, 0 = not run */
    uint32_t selftest_frame_us;     /* time to send one 466x466 frame */
} bsp_display_profile_t;
void bsp_display_profile(bsp_display_profile_t *out);

typedef enum {
    BSP_DISPLAY_ROTATE_0   = 0,
    BSP_DISPLAY_ROTATE_90  = 1,
    BSP_DISPLAY_ROTATE_180 = 2,
    BSP_DISPLAY_ROTATE_270 = 3,
} bsp_display_rotation_t;

/**
 * @brief Create new display panel
 *
 * For maximum flexibility, this function performs only reset and initialization of the display.
 * You must turn on the display explicitly by calling esp_lcd_panel_disp_on_off().
 * The display's backlight is not turned on either. You can use bsp_display_backlight_on/off(),
 * bsp_display_brightness_set() (on supported boards) or implement your own backlight control.
 *
 * If you want to free resources allocated by this function, you can use esp_lcd API, ie.:
 *
 * \code{.c}
 * esp_lcd_panel_del(panel);
 * esp_lcd_panel_io_del(io);
 * spi_bus_free(spi_num_from_configuration);
 * \endcode
 *
 * @param[in]  config    display configuration
 * @param[out] ret_panel esp_lcd panel handle
 * @param[out] ret_io    esp_lcd IO handle
 * @return
 *      - ESP_OK         On success
 *      - Else           esp_lcd failure
 */
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io);

/**
 * @brief Initialize display's brightness
 *
 * Brightness is controlled with PWM signal to a pin controlling backlight.
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   Parameter error
 */
esp_err_t bsp_display_brightness_init(void);

esp_err_t bsp_display_rotation_set(bsp_display_rotation_t rotation);

/**
 * @brief Set display's brightness
 *
 * Brightness is controlled with PWM signal to a pin controlling backlight.
 * Brightness must be already initialized by calling bsp_display_brightness_init() or bsp_display_new()
 *
 * @param[in] brightness_percent Brightness in [%]
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   Parameter error
 */
esp_err_t bsp_display_brightness_set(int brightness_percent);

int bsp_display_brightness_get(void);

/**
 * @brief Turn on display backlight
 *
 * Brightness is controlled with PWM signal to a pin controlling backlight.
 * Brightness must be already initialized by calling bsp_display_brightness_init() or bsp_display_new()
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   Parameter error
 */
esp_err_t bsp_display_backlight_on(void);

/**
 * @brief Turn off display backlight
 *
 * Brightness is controlled with PWM signal to a pin controlling backlight.
 * Brightness must be already initialized by calling bsp_display_brightness_init() or bsp_display_new()
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   Parameter error
 */
esp_err_t bsp_display_backlight_off(void);

#ifdef __cplusplus
}
#endif
