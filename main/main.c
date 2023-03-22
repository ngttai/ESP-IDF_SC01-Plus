#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_st7796.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"

#include "driver/i2c.h"
#include "esp_lcd_touch_ft5x06.h"

static const char *TAG = "SC01_Plus";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if CONFIG_SC01_PLUS_LCD_I80_COLOR_IN_PSRAM
// PCLK frequency can't go too high as the limitation of PSRAM bandwidth
#define SC01_PLUS_LCD_PIXEL_CLOCK_HZ     (2 * 1000 * 1000)
#else
#define SC01_PLUS_LCD_PIXEL_CLOCK_HZ     (10 * 1000 * 1000)
#endif // CONFIG_SC01_PLUS_LCD_I80_COLOR_IN_PSRAM

#define SC01_PLUS_LCD_BK_LIGHT_ON_LEVEL  1
#define SC01_PLUS_LCD_BK_LIGHT_OFF_LEVEL !SC01_PLUS_LCD_BK_LIGHT_ON_LEVEL
#define SC01_PLUS_PIN_NUM_DATA0          9
#define SC01_PLUS_PIN_NUM_DATA1          46
#define SC01_PLUS_PIN_NUM_DATA2          3
#define SC01_PLUS_PIN_NUM_DATA3          8
#define SC01_PLUS_PIN_NUM_DATA4          18
#define SC01_PLUS_PIN_NUM_DATA5          17
#define SC01_PLUS_PIN_NUM_DATA6          16
#define SC01_PLUS_PIN_NUM_DATA7          15
#define SC01_PLUS_PIN_NUM_PCLK           47
#define SC01_PLUS_PIN_NUM_CS             -1
#define SC01_PLUS_PIN_NUM_DC             0
#define SC01_PLUS_PIN_NUM_RST            4
#define SC01_PLUS_PIN_NUM_BK_LIGHT       45

#define SC01_PLUS_LCD_I80_BUS_WIDTH      8

// The pixel number in horizontal and vertical
#define SC01_PLUS_LCD_H_RES              320
#define SC01_PLUS_LCD_V_RES              480
// Bit number used to represent command and parameter
#define SC01_PLUS_LCD_CMD_BITS           8
#define SC01_PLUS_LCD_PARAM_BITS         8

// Touch Config
#define SC01_PLUS_I2C_NUM                 1   // I2C number
#define SC01_PLUS_I2C_SCL                 5
#define SC01_PLUS_I2C_SDA                 6

#define SC01_PLUS_LVGL_TICK_PERIOD_MS    2

// Supported alignment: 16, 32, 64. A higher alignment can enables higher burst transfer size, thus a higher i80 bus throughput.
#define SC01_PLUS_PSRAM_DATA_ALIGNMENT   64

extern void sc01_plus_lvgl_demo_ui(lv_disp_t *disp);

static bool sc01_plus_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void sc01_plus_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

static void sc01_plus_lvgl_touch_cb(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    /* Read touch controller data */
    esp_lcd_touch_read_data(drv->user_data);

    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(drv->user_data, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void sc01_plus_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(SC01_PLUS_LVGL_TICK_PERIOD_MS);
}

void app_main(void)
{
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    static lv_disp_drv_t disp_drv;      // contains callback functions

    ESP_LOGI(TAG, "Turn off LCD backlight");
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << SC01_PLUS_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    gpio_set_level(SC01_PLUS_PIN_NUM_BK_LIGHT, SC01_PLUS_LCD_BK_LIGHT_OFF_LEVEL);

    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = SC01_PLUS_PIN_NUM_DC,
        .wr_gpio_num = SC01_PLUS_PIN_NUM_PCLK,
        .data_gpio_nums = {
            SC01_PLUS_PIN_NUM_DATA0,
            SC01_PLUS_PIN_NUM_DATA1,
            SC01_PLUS_PIN_NUM_DATA2,
            SC01_PLUS_PIN_NUM_DATA3,
            SC01_PLUS_PIN_NUM_DATA4,
            SC01_PLUS_PIN_NUM_DATA5,
            SC01_PLUS_PIN_NUM_DATA6,
            SC01_PLUS_PIN_NUM_DATA7,
        },
        .bus_width = SC01_PLUS_LCD_I80_BUS_WIDTH,
        .max_transfer_bytes = SC01_PLUS_LCD_H_RES * 100 * sizeof(uint16_t),
        .psram_trans_align = SC01_PLUS_PSRAM_DATA_ALIGNMENT,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = SC01_PLUS_PIN_NUM_CS,
        .pclk_hz = SC01_PLUS_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .flags = {
            .swap_color_bytes = !LV_COLOR_16_SWAP, // Swap can be done in LvGL (default) or DMA
        },
        .on_color_trans_done = sc01_plus_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = SC01_PLUS_LCD_CMD_BITS,
        .lcd_param_bits = SC01_PLUS_LCD_PARAM_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_LOGI(TAG, "Install LCD driver of st7796");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = SC01_PLUS_PIN_NUM_RST,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle));

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    // Set inversion, x/y coordinate order, x/y mirror according to your LCD module spec
    // the gap is LCD panel specific, even panels with the same driver IC, can have different gap value
    
    esp_lcd_panel_invert_color(panel_handle, true);
    esp_lcd_panel_mirror(panel_handle, true, false);

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Turn on LCD backlight");
    gpio_set_level(SC01_PLUS_PIN_NUM_BK_LIGHT, SC01_PLUS_LCD_BK_LIGHT_ON_LEVEL);

    esp_lcd_touch_handle_t tp = NULL;
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;

    ESP_LOGI(TAG, "Initialize I2C");

    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SC01_PLUS_I2C_SDA,
        .scl_io_num = SC01_PLUS_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    /* Initialize I2C */
    ESP_ERROR_CHECK(i2c_param_config(SC01_PLUS_I2C_NUM, &i2c_conf));
    ESP_ERROR_CHECK(i2c_driver_install(SC01_PLUS_I2C_NUM, i2c_conf.mode, 0, 0, 0));

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    ESP_LOGI(TAG, "Initialize touch IO (I2C)");

    /* Touch IO handle */
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)SC01_PLUS_I2C_NUM, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = SC01_PLUS_LCD_V_RES,
        .y_max = SC01_PLUS_LCD_H_RES,
        .rst_gpio_num = 4,
        .int_gpio_num = 7,
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    /* Initialize touch */
    ESP_LOGI(TAG, "Initialize touch controller FT5X06");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = NULL;
    lv_color_t *buf2 = NULL;
#if CONFIG_SC01_PLUS_LCD_I80_COLOR_IN_PSRAM
    buf1 = heap_caps_aligned_alloc(SC01_PLUS_PSRAM_DATA_ALIGNMENT, SC01_PLUS_LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    buf1 = heap_caps_malloc(SC01_PLUS_LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
#endif
    assert(buf1);
#if CONFIG_SC01_PLUS_LCD_I80_COLOR_IN_PSRAM
    buf2 = heap_caps_aligned_alloc(SC01_PLUS_PSRAM_DATA_ALIGNMENT, SC01_PLUS_LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    buf2 = heap_caps_malloc(SC01_PLUS_LCD_H_RES * 100 * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
#endif
    assert(buf2);
    ESP_LOGI(TAG, "buf1@%p, buf2@%p", buf1, buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, SC01_PLUS_LCD_H_RES * 100);

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SC01_PLUS_LCD_H_RES;
    disp_drv.ver_res = SC01_PLUS_LCD_V_RES;
    disp_drv.flush_cb = sc01_plus_lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &sc01_plus_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, SC01_PLUS_LVGL_TICK_PERIOD_MS * 1000));

    static lv_indev_drv_t indev_drv;    // Input device driver (Touch)
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = sc01_plus_lvgl_touch_cb;
    indev_drv.user_data = tp;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Display LVGL animation");
    sc01_plus_lvgl_demo_ui(disp);

    while (1) {
        // raise the task priority of LVGL and/or reduce the handler period can improve the performance
        vTaskDelay(pdMS_TO_TICKS(10));
        // The task running lv_timer_handler should have lower priority than that running `lv_tick_inc`
        lv_timer_handler();
    }

}