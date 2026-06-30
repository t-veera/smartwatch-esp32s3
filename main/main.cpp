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
#include "fpw_log.h"
#include "fpw_nav.h"

#include "driver/gpio.h"
#include "esp_console.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <dirent.h>

namespace {
constexpr char TAG[] = "fpw";

void imu_task(void *)
{
    while (true) {
        fpw_imu_service();   // ~50 Hz step detection
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Poll the AXP2101 PWR button; a short press starts/stops a Kart Log session.
// Debounced so one press fires once. Works regardless of display state.
void button_task(void *)
{
    TickType_t last = 0;
    while (true) {
        if (bsp_power_poll_pwr_button_short()) {
            TickType_t now = xTaskGetTickCount();
            if (now - last > pdMS_TO_TICKS(600)) {
                last = now;
                fpw_log_toggle();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(80));
    }
}

// Poll the BOOT button (GPIO0); a press captures the calibration reference
// (only valid while stationary and not logging).
void boot_button_task(void *)
{
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << GPIO_NUM_0;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io);

    bool prev_high = true;
    TickType_t last = 0;
    while (true) {
        bool low = (gpio_get_level(GPIO_NUM_0) == 0);
        if (low && prev_high) {   // falling edge = press
            TickType_t now = xTaskGetTickCount();
            if (now - last > pdMS_TO_TICKS(700)) {
                last = now;
                fpw_log_calibrate();   // ~1.5 s sampling window
            }
        }
        prev_high = !low;
        vTaskDelay(pdMS_TO_TICKS(40));
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

// List SD log files and print the head of the most recent kart_*.csv.
int cmd_logdump(int, char **)
{
    DIR *d = opendir("/sdcard");
    if (!d) {
        printf("opendir /sdcard failed (no card?)\n");
        return 0;
    }
    char latest[64] = "";
    struct dirent *e;
    while ((e = readdir(d)) != nullptr) {
        if (strncmp(e->d_name, "kart_", 5) == 0) {
            printf("  %s\n", e->d_name);
            if (strcmp(e->d_name, latest) > 0) {
                strncpy(latest, e->d_name, sizeof(latest) - 1);
            }
        }
    }
    closedir(d);
    if (!latest[0]) {
        printf("no kart_*.csv files\n");
        return 0;
    }
    char path[80];
    snprintf(path, sizeof(path), "/sdcard/%s", latest);
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("open failed: %s\n", path);
        return 0;
    }
    printf("=== HEAD %s ===\n", path);
    char line[160];
    int n = 0;
    while (n < 25 && fgets(line, sizeof(line), f)) {
        printf("%s", line);
        n++;
    }
    fseek(f, 0, SEEK_END);
    printf("=== total %ld bytes ===\n", ftell(f));
    fclose(f);
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
    const esp_console_cmd_t c_dump = {
        .command = "logdump", .help = "List SD logs + head of the latest CSV", .hint = nullptr,
        .func = &cmd_logdump, .argtable = nullptr, .func_w_context = nullptr, .context = nullptr
    };
    esp_console_cmd_register(&c_time);
    esp_console_cmd_register(&c_set);
    esp_console_cmd_register(&c_dump);
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
    xTaskCreate(boot_button_task, "bootbtn", 4096, nullptr, 4, nullptr);
    fpw_power_init();
    fpw_log_init();   // mount SD for the Kart Telemetry Logger (PWR button start/stop)

    ESP_LOGI(TAG, "Step 4 up — watchface + sleep/wake running.");
}
