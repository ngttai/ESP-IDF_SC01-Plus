idf_component_register(
    SRCS "wt32_sc01_plus.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    REQUIRES driver
    PRIV_REQUIRES fatfs esp_timer esp_lcd esp_lcd_touch esp_lcd_st7796
)
