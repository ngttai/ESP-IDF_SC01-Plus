/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "lv_demos.h"
#include "bsp/esp-bsp.h"
#include "sdmmc_cmd.h" // for sdmmc_card_print_info

static char *TAG = "app_main";

#define LOG_MEM_INFO    (0)

void app_main(void)
{
    lv_disp_t * disp;
    bsp_i2c_init();
    disp = bsp_display_start();

    bsp_display_rotate(disp, LV_DISP_ROT_270);

#if CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR
    ESP_LOGI(TAG, "Avoid lcd tearing effect");
#if CONFIG_BSP_DISPLAY_LVGL_FULL_REFRESH
    ESP_LOGI(TAG, "LVGL full-refresh");
#elif CONFIG_BSP_DISPLAY_LVGL_DIRECT_MODE
    ESP_LOGI(TAG, "LVGL direct-mode");
#endif
#endif

    ESP_LOGI(TAG, "Display LVGL demo");
    bsp_display_lock(0);
#if CONFIG_LV_USE_DEMO_WIDGETS
    lv_demo_widgets();      /* A widgets example */
#elif CONFIG_LV_USE_DEMO_MUSIC
    lv_demo_music();        /* A modern, smartphone-like music player demo. */
#elif CONFIG_LV_USE_DEMO_STRESS
    lv_demo_stress();       /* A stress test for LVGL. */
#elif CONFIG_LV_USE_DEMO_BENCHMARK
    lv_demo_benchmark();    /* A demo to measure the performance of LVGL or to compare different settings. */
#else
#error "Not Supported!"
#endif

    bsp_display_unlock();
    bsp_display_brightness_set(20);

    // Mount uSD card
    if (ESP_OK == bsp_sdcard_mount()) {
        sdmmc_card_print_info(stdout, bsp_sdcard);
        FILE *f = fopen(BSP_MOUNT_POINT "/hello.txt", "w");
        fprintf(f, "Hello %s!\n", bsp_sdcard->cid.name);
        fclose(f);
        bsp_sdcard_unmount();
    }

#if LOG_MEM_INFO
    static char buffer[128];    /* Make sure buffer is enough for `sprintf` */
    while (1) {
        sprintf(buffer, "   Biggest /     Free /    Total\n"
                "\t  SRAM : [%8d / %8d / %8d]\n"
                "\t PSRAM : [%8d / %8d / %8d]",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_total_size(MALLOC_CAP_INTERNAL),
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM),
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
        ESP_LOGI("MEM", "%s", buffer);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
#endif
}
