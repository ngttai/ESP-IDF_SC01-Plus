/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/sdspi_host.h"
#include "lvgl.h"

/**************************************************************************************************
 *  ESP-BOX pinout
 **************************************************************************************************/
/* I2C */
#define BSP_I2C_SCL           (GPIO_NUM_5)
#define BSP_I2C_SDA           (GPIO_NUM_6)

/* Display */
#define BSP_LCD_WIDTH        (8) // i80 bus width: 8bit MCU (8080)
#define BSP_LCD_DB0          (GPIO_NUM_9)  // LCD data interface, 8bit MCU (8080)
#define BSP_LCD_DB1          (GPIO_NUM_46) // LCD data interface, 8bit MCU (8080)
#define BSP_LCD_DB2          (GPIO_NUM_3)  // LCD data interface, 8bit MCU (8080)
#define BSP_LCD_DB3          (GPIO_NUM_8)  // LCD data interface, 8bit MCU (8080)
#define BSP_LCD_DB4          (GPIO_NUM_18) // LCD data interface, 8bit MCU (8080)
#define BSP_LCD_DB5          (GPIO_NUM_17) // LCD data interface, 8bit MCU (8080)
#define BSP_LCD_DB6          (GPIO_NUM_16) // LCD data interface, 8bit MCU (8080)
#define BSP_LCD_DB7          (GPIO_NUM_15) // LCD data interface, 8bit MCU (8080)
#define BSP_LCD_CS           (GPIO_NUM_NC) //
#define BSP_LCD_DC           (GPIO_NUM_0)  // Command/Data selection
#define BSP_LCD_WR           (GPIO_NUM_47) // Write clock
#define BSP_LCD_RD           (GPIO_NUM_NC) // 
#define BSP_LCD_RST          (GPIO_NUM_4)  // LCD reset, multiplexed with touch reset
#define BSP_LCD_TE           (GPIO_NUM_48) // Frame sync
#define BSP_LCD_BACKLIGHT    (GPIO_NUM_45) // Backlight control, active high
#define BSP_LCD_TP_INT       (GPIO_NUM_7)  // Touch interrupt
#define BSP_LCD_TP_RST       (GPIO_NUM_NC) // Disable (GPIO_NUM_4)  // Touch reset, multiplexed with LCD reset. 

/* uSD card */
#define BSP_SD_MOSI            (GPIO_NUM_40)
#define BSP_SD_MISO            (GPIO_NUM_38)
#define BSP_SD_CLK             (GPIO_NUM_39)
#define BSP_SD_CS              (GPIO_NUM_41)

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 *
 * I2C interface
 *
 * There are multiple devices connected to I2C peripheral:)
 *  - LCD Touch controller
 *
 * After initialization of I2C, use BSP_I2C_NUM macro when creating I2C devices drivers ie.:
 * \code{.c}
 * es8311_handle_t es8311_dev = es8311_create(BSP_I2C_NUM, ES8311_ADDRRES_0);
 * \endcode
 **************************************************************************************************/
#define BSP_I2C_NUM             1
#define BSP_I2C_CLK_SPEED_HZ    400000

/**
 * @brief Init I2C driver
 *
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief Deinit I2C driver and free its resources
 *
 */
esp_err_t bsp_i2c_deinit(void);

/**************************************************************************************************
 *
 * uSD card
 *
 * After mounting the uSD card, it can be accessed with stdio functions ie.:
 * \code{.c}
 * FILE* f = fopen(BSP_MOUNT_POINT"/hello.txt", "w");
 * fprintf(f, "Hello %s!\n", bsp_sdcard->cid.name);
 * fclose(f);
 * \endcode
 **************************************************************************************************/
#define BSP_MOUNT_POINT      CONFIG_BSP_SD_MOUNT_POINT
extern sdmmc_card_t *bsp_sdcard;

/**
 * @brief Mount microSD card to virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_sdmmc_mount was already called
 *      - ESP_ERR_NO_MEM if memory cannot be allocated
 *      - ESP_FAIL if partition cannot be mounted
 *      - other error codes from SDMMC or SPI drivers, SDMMC protocol, or FATFS drivers
 */
esp_err_t bsp_sdcard_mount(void);

/**
 * @brief Unmount microSD card from virtual file system
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_NOT_FOUND if the partition table does not contain FATFS partition with given label
 *      - ESP_ERR_INVALID_STATE if esp_vfs_fat_spiflash_mount was already called
 *      - ESP_ERR_NO_MEM if memory can not be allocated
 *      - ESP_FAIL if partition can not be mounted
 *      - other error codes from wear levelling library, SPI flash driver, or FATFS drivers
 */
esp_err_t bsp_sdcard_unmount(void);

/**************************************************************************************************
 *
 * LCD interface
 *
 * This display has 3.5inch and ST7796 display controller.
 * It features 16-bit colors, 320x480 resolution and capacitive touch controller FT5x06.
 *
 * LVGL is used as graphics library. LVGL is NOT thread safe, therefore the user must take LVGL mutex
 * by calling bsp_display_lock() before calling and LVGL API (lv_...) and then give the mutex with
 * bsp_display_unlock().
 *
 * Display's backlight must be enabled explicitly by calling bsp_display_backlight_on()
 **************************************************************************************************/
#define BSP_LCD_H_RES              (320)
#define BSP_LCD_V_RES              (480)
#define BSP_LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)

/**
 * @brief Initialize display
 *
 * This function initializes SPI, display controller and starts LVGL handling task.
 * LCD backlight must be enabled separately by calling bsp_display_brightness_set()
 *
 * @return Pointer to LVGL display or NULL when error occured
 */
lv_disp_t *bsp_display_start(void);

/**
 * @brief Take LVGL mutex
 *
 * @param timeout_ms Timeout in [ms]. 0 will block indefinitely.
 * @return true  Mutex was taken
 * @return false Mutex was NOT taken
 */
bool bsp_display_lock(uint32_t timeout_ms);

/**
 * @brief Give LVGL mutex
 *
 */
void bsp_display_unlock(void);

/**
 * @brief Set display's brightness
 *
 * Brightness is controlled with PWM signal to a pin controling backlight.
 *
 * @param[in] brightness_percent Brightness in [%]
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   Parameter error
 */
esp_err_t bsp_display_brightness_set(int brightness_percent);

/**
 * @brief Turn on display backlight
 *
 * Display must be already initialized by calling bsp_display_start()
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   Parameter error
 */
esp_err_t bsp_display_backlight_on(void);

/**
 * @brief Turn off display backlight
 *
 * Display must be already initialized by calling bsp_display_start()
 *
 * @return
 *      - ESP_OK                On success
 *      - ESP_ERR_INVALID_ARG   Parameter error
 */
esp_err_t bsp_display_backlight_off(void);

/**
 * @brief Rotate screen
 *
 * Display must be already initialized by calling bsp_display_start()
 *
 * @param[in] disp Pointer to LVGL display
 * @param[in] rotation Angle of the display rotation
 */
void bsp_display_rotate(lv_disp_t *disp, lv_disp_rot_t rotation);

#ifdef __cplusplus
}
#endif
