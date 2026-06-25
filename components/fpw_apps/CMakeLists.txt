idf_component_register(
    SRCS "fpw_nav.cpp" "watchface/ui_watchface.cpp" "steps/ui_steps.cpp" "trackpad/ui_trackpad.cpp"
    INCLUDE_DIRS "." "watchface" "steps" "trackpad"
    REQUIRES esp32_s3_touch_amoled_2_06 fpw_ui fpw_hal
)
