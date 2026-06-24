idf_component_register(
    SRCS "fpw_pmic.c" "fpw_rtc.c" "fpw_imu.c"
    INCLUDE_DIRS "include"
    REQUIRES esp32_s3_touch_amoled_2_06 waveshare__qmi8658 esp_driver_i2c esp_timer
)
