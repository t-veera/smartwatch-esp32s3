#include "fpw_log.h"
#include "fpw_imu.h"
#include "fpw_rtc.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <unistd.h>   // fsync

namespace {
constexpr char TAG[] = "fpw_log";

constexpr int SAMPLE_HZ           = 100;            // configurable sample rate
constexpr int FLUSH_EVERY_SAMPLES = SAMPLE_HZ * 2;  // periodic flush (~2 s) safety net
constexpr int CAL_SAMPLES         = SAMPLE_HZ * 3 / 2;  // ~1.5 s calibration window

volatile bool     s_active = false;
volatile bool     s_sd_ok  = false;
TaskHandle_t      s_task   = nullptr;
volatile uint32_t s_count  = 0;
int64_t           s_start_us = 0;
char              s_fname[48] = "";

// --- Calibration (session orientation correction) ---
volatile bool s_calibrated = false;
volatile bool s_cal_rejected = false;   // a PWR start was refused (uncalibrated)
float s_R[9];                            // rotation: body -> levelled frame
float s_gbias[3];                        // gyro resting bias (deg/s)
float s_ref_accel[3];                    // averaged at-rest accel (g)
float s_ref_gyro[3];                     // averaged at-rest gyro (deg/s)
char  s_cal_time[24] = "none";

// Rotation matrix (row-major) that rotates unit vector a onto unit vector b.
void rot_from_vectors(const float a[3], const float b[3], float R[9])
{
    float v[3] = { a[1] * b[2] - a[2] * b[1],
                   a[2] * b[0] - a[0] * b[2],
                   a[0] * b[1] - a[1] * b[0] };           // a x b
    float c = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];    // a . b
    if (c > 0.9999f) {                                    // already aligned
        R[0]=1;R[1]=0;R[2]=0; R[3]=0;R[4]=1;R[5]=0; R[6]=0;R[7]=0;R[8]=1;
        return;
    }
    if (c < -0.9999f) {                                   // opposite (upside down)
        R[0]=1;R[1]=0;R[2]=0; R[3]=0;R[4]=-1;R[5]=0; R[6]=0;R[7]=0;R[8]=-1;
        return;
    }
    float k = 1.0f / (1.0f + c);   // Rodrigues: R = I + [v]x + [v]x^2 * k
    R[0] = 1 - (v[1]*v[1] + v[2]*v[2]) * k;
    R[1] = -v[2] + v[0]*v[1] * k;
    R[2] =  v[1] + v[0]*v[2] * k;
    R[3] =  v[2] + v[0]*v[1] * k;
    R[4] = 1 - (v[0]*v[0] + v[2]*v[2]) * k;
    R[5] = -v[0] + v[1]*v[2] * k;
    R[6] = -v[1] + v[0]*v[2] * k;
    R[7] =  v[0] + v[1]*v[2] * k;
    R[8] = 1 - (v[0]*v[0] + v[1]*v[1]) * k;
}

void apply_rot(const float R[9], const float in[3], float out[3])
{
    out[0] = R[0]*in[0] + R[1]*in[1] + R[2]*in[2];
    out[1] = R[3]*in[0] + R[4]*in[1] + R[5]*in[2];
    out[2] = R[6]*in[0] + R[7]*in[1] + R[8]*in[2];
}

// Next kart_unset_NNN.csv index when the clock is invalid.
int next_seq_index()
{
    int max_idx = 0;
    DIR *d = opendir(BSP_SD_MOUNT_POINT);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != nullptr) {
            int idx = 0;
            if (sscanf(e->d_name, "kart_unset_%d.csv", &idx) == 1 && idx > max_idx) {
                max_idx = idx;
            }
        }
        closedir(d);
    }
    return max_idx + 1;
}

void build_filename()
{
    struct tm t;
    if (fpw_rtc_get_time(&t) == ESP_OK && fpw_rtc_time_valid()) {
        strftime(s_fname, sizeof(s_fname), "kart_%Y%m%d_%H%M%S.csv", &t);
    } else {
        snprintf(s_fname, sizeof(s_fname), "kart_unset_%03d.csv", next_seq_index());
    }
}

void logging_task(void *)
{
    build_filename();
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

    // Calibration metadata (auditable; analysis tools skip '#' lines).
    fputs("# kart telemetry log\n", f);
    fprintf(f, "# calibrated_at: %s\n", s_cal_time);
    fprintf(f, "# ref_accel_g: %.4f,%.4f,%.4f\n", s_ref_accel[0], s_ref_accel[1], s_ref_accel[2]);
    fprintf(f, "# ref_gyro_dps: %.4f,%.4f,%.4f\n", s_ref_gyro[0], s_ref_gyro[1], s_ref_gyro[2]);
    fprintf(f, "# rotation: %.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n",
            s_R[0], s_R[1], s_R[2], s_R[3], s_R[4], s_R[5], s_R[6], s_R[7], s_R[8]);
    fputs("t_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,"
          "cax_g,cay_g,caz_g,cgx_dps,cgy_dps,cgz_dps\n", f);

    s_start_us = esp_timer_get_time();
    s_count = 0;
    ESP_LOGI(TAG, "REC start -> %s @ %d Hz", path, SAMPLE_HZ);

    const TickType_t period = pdMS_TO_TICKS(1000 / SAMPLE_HZ);
    TickType_t last = xTaskGetTickCount();
    while (s_active) {
        vTaskDelayUntil(&last, period);
        float ax, ay, az, gx, gy, gz;
        if (fpw_imu_read_all(&ax, &ay, &az, &gx, &gy, &gz) != ESP_OK) {
            continue;
        }
        // Apply the session correction: de-bias the gyro, then rotate both
        // accel and gyro into the levelled frame.
        float ar[3] = { ax, ay, az };
        float gr[3] = { gx - s_gbias[0], gy - s_gbias[1], gz - s_gbias[2] };
        float ac[3], gc[3];
        apply_rot(s_R, ar, ac);
        apply_rot(s_R, gr, gc);

        uint32_t t_ms = static_cast<uint32_t>((esp_timer_get_time() - s_start_us) / 1000);
        fprintf(f, "%lu,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f,%.4f,%.4f,%.4f,%.2f,%.2f,%.2f\n",
                static_cast<unsigned long>(t_ms),
                ax, ay, az, gx, gy, gz,
                ac[0], ac[1], ac[2], gc[0], gc[1], gc[2]);
        s_count++;
        if (s_count % FLUSH_EVERY_SAMPLES == 0) {
            fflush(f);
            fsync(fileno(f));
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

extern "C" bool fpw_log_calibrate(void)
{
    if (s_active) {
        ESP_LOGW(TAG, "stop logging before calibrating");
        return false;
    }
    double sa[3] = {0}, sg[3] = {0}, sg_mag = 0;
    int got = 0;
    TickType_t last = xTaskGetTickCount();
    for (int i = 0; i < CAL_SAMPLES; i++) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000 / SAMPLE_HZ));
        float ax, ay, az, gx, gy, gz;
        if (fpw_imu_read_all(&ax, &ay, &az, &gx, &gy, &gz) == ESP_OK) {
            sa[0] += ax; sa[1] += ay; sa[2] += az;
            sg[0] += gx; sg[1] += gy; sg[2] += gz;
            sg_mag += sqrtf(gx*gx + gy*gy + gz*gz);
            got++;
        }
    }
    if (got < CAL_SAMPLES / 2) {
        ESP_LOGE(TAG, "calibration failed (IMU read)");
        return false;
    }
    float ra[3] = { (float)(sa[0]/got), (float)(sa[1]/got), (float)(sa[2]/got) };
    float rg[3] = { (float)(sg[0]/got), (float)(sg[1]/got), (float)(sg[2]/got) };

    // Reject if the device was clearly moving during the window.
    if (sg_mag / got > 8.0f) {
        ESP_LOGW(TAG, "not stationary (%.1f dps) — hold still and retry", sg_mag / got);
        return false;
    }
    float amag = sqrtf(ra[0]*ra[0] + ra[1]*ra[1] + ra[2]*ra[2]);
    if (amag < 0.5f) {
        ESP_LOGE(TAG, "calibration failed (accel %.2f g)", amag);
        return false;
    }
    // Map measured gravity onto -Z so a level mount stays neutral.
    float an[3] = { ra[0]/amag, ra[1]/amag, ra[2]/amag };
    float down[3] = { 0.0f, 0.0f, -1.0f };
    rot_from_vectors(an, down, s_R);

    memcpy(s_ref_accel, ra, sizeof(ra));
    memcpy(s_ref_gyro, rg, sizeof(rg));
    memcpy(s_gbias, rg, sizeof(rg));

    struct tm t;
    if (fpw_rtc_get_time(&t) == ESP_OK && fpw_rtc_time_valid()) {
        strftime(s_cal_time, sizeof(s_cal_time), "%Y-%m-%d %H:%M:%S", &t);
    } else {
        strncpy(s_cal_time, "unset", sizeof(s_cal_time));
    }
    s_calibrated = true;
    s_cal_rejected = false;
    ESP_LOGI(TAG, "calibrated: accel(%.3f,%.3f,%.3f) gyro_bias(%.2f,%.2f,%.2f)",
             ra[0], ra[1], ra[2], rg[0], rg[1], rg[2]);
    return true;
}

extern "C" void fpw_log_toggle(void)
{
    if (!s_active) {
        if (!s_calibrated) {
            s_cal_rejected = true;
            ESP_LOGW(TAG, "not calibrated — press BOOT while stationary first");
            return;
        }
        if (!s_sd_ok) {
            s_sd_ok = (bsp_sdcard_mount() == ESP_OK);
            if (!s_sd_ok) {
                ESP_LOGW(TAG, "no SD card — cannot log");
                return;
            }
        }
        s_active = true;
        if (xTaskCreate(logging_task, "kartlog", 8192, nullptr, 6, &s_task) != pdPASS) {
            s_active = false;
            ESP_LOGE(TAG, "logging task create failed");
        }
    } else {
        s_active = false;   // the task flushes, closes and exits itself
    }
}

extern "C" bool        fpw_log_active(void)        { return s_active; }
extern "C" bool        fpw_log_sd_ok(void)         { return s_sd_ok; }
extern "C" bool        fpw_log_is_calibrated(void) { return s_calibrated; }
extern "C" bool        fpw_log_cal_rejected(void)  { return s_cal_rejected; }
extern "C" uint32_t    fpw_log_sample_count(void)  { return s_count; }
extern "C" const char *fpw_log_filename(void)      { return s_fname; }

extern "C" uint32_t fpw_log_elapsed_ms(void)
{
    return s_active ? static_cast<uint32_t>((esp_timer_get_time() - s_start_us) / 1000) : 0;
}
