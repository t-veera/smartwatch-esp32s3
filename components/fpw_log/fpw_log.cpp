#include "fpw_log.h"
#include "fpw_imu.h"
#include "fpw_rtc.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>   // fsync

namespace {
constexpr char TAG[] = "fpw_log";

constexpr int SAMPLE_HZ           = 100;            // configurable sample rate
constexpr int FLUSH_EVERY_SAMPLES = SAMPLE_HZ * 2;  // periodic flush (~2 s) safety net

volatile bool     s_active = false;
volatile bool     s_sd_ok  = false;
TaskHandle_t      s_task   = nullptr;
volatile uint32_t s_count  = 0;
int64_t           s_start_us = 0;
char              s_fname[40] = "";

void logging_task(void *)
{
    // Filename from the RTC (held in IST): kart_YYYYMMDD_HHMMSS.csv
    struct tm t;
    if (fpw_rtc_get_time(&t) != ESP_OK) {
        memset(&t, 0, sizeof(t));
    }
    strftime(s_fname, sizeof(s_fname), "kart_%Y%m%d_%H%M%S.csv", &t);

    char path[80];
    snprintf(path, sizeof(path), "%s/%s", BSP_SD_MOUNT_POINT, s_fname);

    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "could not open %s", path);
        s_active = false;
        s_task = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    static char iobuf[4096];
    setvbuf(f, iobuf, _IOFBF, sizeof(iobuf));
    fputs("t_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps\n", f);

    s_start_us = esp_timer_get_time();
    s_count = 0;
    ESP_LOGI(TAG, "REC start -> %s @ %d Hz", path, SAMPLE_HZ);

    const TickType_t period = pdMS_TO_TICKS(1000 / SAMPLE_HZ);
    TickType_t last = xTaskGetTickCount();
    while (s_active) {
        vTaskDelayUntil(&last, period);
        float ax, ay, az, gx, gy, gz;
        if (fpw_imu_read_all(&ax, &ay, &az, &gx, &gy, &gz) == ESP_OK) {
            uint32_t t_ms = static_cast<uint32_t>((esp_timer_get_time() - s_start_us) / 1000);
            fprintf(f, "%lu,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f\n",
                    static_cast<unsigned long>(t_ms), ax, ay, az, gx, gy, gz);
            s_count++;
            if (s_count % FLUSH_EVERY_SAMPLES == 0) {
                fflush(f);
                fsync(fileno(f));
            }
        }
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);
    ESP_LOGI(TAG, "REC stop: %lu samples in %.1fs -> %s",
             static_cast<unsigned long>(s_count),
             (esp_timer_get_time() - s_start_us) / 1e6, path);
    s_task = nullptr;
    vTaskDelete(nullptr);
}
}  // namespace

extern "C" void fpw_log_init(void)
{
    s_sd_ok = (bsp_sdcard_mount() == ESP_OK);
    if (s_sd_ok) {
        ESP_LOGI(TAG, "SD card mounted at %s", BSP_SD_MOUNT_POINT);
    } else {
        ESP_LOGW(TAG, "SD card not mounted (insert a card)");
    }
}

extern "C" void fpw_log_toggle(void)
{
    if (!s_sd_ok) {
        // A card may have been inserted after boot; try once more.
        s_sd_ok = (bsp_sdcard_mount() == ESP_OK);
        if (!s_sd_ok) {
            ESP_LOGW(TAG, "no SD card — cannot log");
            return;
        }
    }
    if (!s_active) {
        s_active = true;
        if (xTaskCreate(logging_task, "kartlog", 8192, nullptr, 6, &s_task) != pdPASS) {
            s_active = false;
            ESP_LOGE(TAG, "logging task create failed");
        }
    } else {
        s_active = false;   // the task flushes, closes and exits itself
    }
}

extern "C" bool        fpw_log_active(void)       { return s_active; }
extern "C" bool        fpw_log_sd_ok(void)        { return s_sd_ok; }
extern "C" uint32_t    fpw_log_sample_count(void) { return s_count; }
extern "C" const char *fpw_log_filename(void)     { return s_fname; }

extern "C" uint32_t fpw_log_elapsed_ms(void)
{
    return s_active ? static_cast<uint32_t>((esp_timer_get_time() - s_start_us) / 1000) : 0;
}
