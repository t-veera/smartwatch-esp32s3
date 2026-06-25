// Field Pocket Watch — firmware entry point.
//
// Step 3: the watchface. Bring up the display and peripherals, then build the
// watchface home screen (time/date/battery/steps). A low-priority task feeds
// the IMU step detector; the `time`/`settime` console (USB-Serial/JTAG) remains
// available for setting the RTC.

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "fpw_imu.h"
#include "fpw_pmic.h"
#include "fpw_power.h"
#include "fpw_rtc.h"
#include "fpw_nav.h"

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

void imu_task(void *)
{
    while (true) {
        fpw_imu_service();   // ~50 Hz step detection
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Poll the AXP2101 side button; a short press opens the Trackpad app.
void button_task(void *)
{
    while (true) {
        if (bsp_power_poll_pwr_button_short()) {
            fpw_nav_toggle_trackpad();
        }
        vTaskDelay(pdMS_TO_TICKS(80));
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
    ESP_LOGI(TAG, "Field Pocket Watch - Step 3 watchface");

    // Power first: make sure the panel/touch rails are on before the BSP inits
    // the FT3168. A previous screen-off may have gated the AXP2101 ALDO rails,
    // and the PMU keeps that state across a CPU reset — which would otherwise
    // brick touch init in bsp_display_start() and boot-loop.
    bsp_i2c_init();
    fpw_pmic_init();
    bsp_power_enable_aldo1(true);
    bsp_power_enable_aldo2(true);
    bsp_power_enable_aldo3(true);
    bsp_power_enable_aldo4(true);
    vTaskDelay(pdMS_TO_TICKS(120));   // let the rails and FT3168 settle

    lv_display_t *disp = bsp_display_start();
    if (disp == nullptr) {
        ESP_LOGE(TAG, "bsp_display_start() failed");
    }

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

    if (disp != nullptr) {
        bsp_display_lock(0);
        fpw_nav_init();   // watchface + app screens + swipe navigation
        bsp_display_unlock();
    }

    start_console();
    xTaskCreate(imu_task, "imu", 4096, nullptr, 2, nullptr);  // below the LVGL render task
    xTaskCreate(button_task, "btn", 4096, nullptr, 4, nullptr);
    fpw_power_init();

    ESP_LOGI(TAG, "Step 4 up — watchface + sleep/wake running.");
}
