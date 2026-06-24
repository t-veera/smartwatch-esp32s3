idf_component_register(
    SRCS "fpw_power.cpp"
    INCLUDE_DIRS "include"
    REQUIRES esp32_s3_touch_amoled_2_06 fpw_hal esp_timer
)
