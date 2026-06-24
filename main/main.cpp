// Field Pocket Watch — firmware entry point.
//
// Step 2: peripheral bring-up. Initialize the PCF85063 RTC, AXP2101 battery
// gauge and QMI8658 IMU, then log all three to serial once a second. A small
// console (over USB-Serial/JTAG) exposes `time` and `settime` so the RTC can
// be set from a terminal.

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "fpw_imu.h"
#include "fpw_pmic.h"
#include "fpw_rtc.h"

#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace {
constexpr char TAG[] = "fpw";

void show_status_label()
{
    bsp_display_lock(0);
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Field Pocket Watch\nStep 2 - sensors on serial");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_center(label);
    bsp_display_unlock();
}

void sensor_task(void *)
{
    TickType_t last_log = xTaskGetTickCount();
    while (true) {
        fpw_imu_service();  // feed the step detector at ~50 Hz

        if (xTaskGetTickCount() - last_log >= pdMS_TO_TICKS(1000)) {
            last_log = xTaskGetTickCount();

            struct tm t;
            char ts[24] = "----";
            if (fpw_rtc_get_time(&t) == ESP_OK) {
                strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);
            }
            float ax = 0, ay = 0, az = 0;
            fpw_imu_read_accel_mg(&ax, &ay, &az);

            ESP_LOGI(TAG,
                     "RTC %s%s | batt %d%% (%dmV)%s | accel [%.0f,%.0f,%.0f]mg | steps %u",
                     ts, fpw_rtc_time_valid() ? "" : " (unset)",
                     fpw_pmic_battery_percent(), fpw_pmic_battery_mv(),
                     fpw_pmic_is_charging() ? " CHG" : "",
                     ax, ay, az, static_cast<unsigned>(fpw_imu_step_count()));
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

int cmd_time(int, char **)
{
    struct tm t;
    if (fpw_rtc_get_time(&t) == ESP_OK) {
        char buf[24];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
        printf("RTC: %s (%s)\n", buf, fpw_rtc_time_valid() ? "valid" : "unset");
    } else {
        printf("RTC read failed\n");
    }
    return 0;
}

int cmd_settime(int argc, char **argv)
{
    if (argc != 7) {
        printf("usage: settime YYYY MM DD HH MM SS\n");
        return 1;
    }
    struct tm t = {};
    t.tm_year = atoi(argv[1]) - 1900;
    t.tm_mon  = atoi(argv[2]) - 1;
    t.tm_mday = atoi(argv[3]);
    t.tm_hour = atoi(argv[4]);
    t.tm_min  = atoi(argv[5]);
    t.tm_sec  = atoi(argv[6]);

    // Normalize to fill in tm_wday (PCF85063 stores weekday explicitly).
    time_t tt = mktime(&t);
    localtime_r(&tt, &t);

    if (fpw_rtc_set_time(&t) == ESP_OK) {
        printf("RTC set ok\n");
        cmd_time(0, nullptr);
    } else {
        printf("RTC set failed\n");
    }
    return 0;
}

void start_console()
{
    esp_console_repl_t *repl = nullptr;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "fpw>";
    esp_console_dev_usb_serial_jtag_config_t hw = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();

    if (esp_console_new_repl_usb_serial_jtag(&hw, &repl_cfg, &repl) != ESP_OK) {
        ESP_LOGW(TAG, "console init failed; settime unavailable");
        return;
    }
    const esp_console_cmd_t c_time = {
        .command = "time", .help = "Print the RTC time", .hint = nullptr,
        .func = &cmd_time, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr
    };
    const esp_console_cmd_t c_set = {
        .command = "settime", .help = "settime YYYY MM DD HH MM SS", .hint = nullptr,
        .func = &cmd_settime, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr
    };
    esp_console_cmd_register(&c_time);
    esp_console_cmd_register(&c_set);
    esp_console_start_repl(repl);
}
}  // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Field Pocket Watch - Step 2 peripheral bring-up");

    lv_display_t *disp = bsp_display_start();
    if (disp == nullptr) {
        ESP_LOGE(TAG, "bsp_display_start() failed");
    } else {
        show_status_label();
    }

    // Peripherals share the BSP I2C bus (touch already brought it up).
    bsp_i2c_init();
    fpw_pmic_init();
    fpw_rtc_init();
    fpw_imu_init();

    if (!fpw_rtc_time_valid()) {
        struct tm seed = {};
        seed.tm_year = 2026 - 1900;
        seed.tm_mon  = 0;
        seed.tm_mday = 1;
        time_t tt = mktime(&seed);
        localtime_r(&tt, &seed);
        fpw_rtc_set_time(&seed);
        ESP_LOGW(TAG, "RTC was unset; seeded 2026-01-01. Use 'settime YYYY MM DD HH MM SS'.");
    }

    start_console();
    xTaskCreate(sensor_task, "sensors", 4096, nullptr, 3, nullptr);

    ESP_LOGI(TAG, "Step 2 up. Type 'time' or 'settime ...' on the console.");
}
