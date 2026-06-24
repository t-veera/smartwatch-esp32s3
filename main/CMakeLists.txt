idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp32_s3_touch_amoled_2_06 fpw_hal console esp_driver_usb_serial_jtag
)
