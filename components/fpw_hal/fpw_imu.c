// fpw_imu — QMI8658 accelerometer + simple software pedometer.
//
// Uses the waveshare/qmi8658 driver, initialized on the BSP's shared
// i2c_master bus. Step detection is a peak detector on the accel-magnitude AC
// component with a refractory period — good enough to prove the sensor and to
// count walking steps; Step 6 can refine it.

#include "fpw_imu.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "qmi8658.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "fpw_imu";

static qmi8658_dev_t s_imu;
static bool s_ready = false;
static volatile uint32_t s_steps = 0;
static volatile bool s_motion_event = false;
#define WAKE_MOTION_MG 250.0f   // AC magnitude that counts as a deliberate move

// Peak-detector state (magnitudes in mg, ~1000 mg = 1 g at rest).
static float    s_baseline   = 1000.0f;
static bool     s_armed      = true;
static uint32_t s_last_step_ms = 0;
#define STEP_PEAK_MG        180.0f   // AC magnitude that counts as a step peak
#define STEP_REARM_MG        80.0f   // fall below this to re-arm for next step
#define STEP_REFRACTORY_MS   300     // min spacing between counted steps

// Day stats derived from steps.
#define STRIDE_M           0.72f
#define MAX_PLAUSIBLE_MPS  6.0f      // reject noise faster than ~21 km/h
static volatile float s_distance_m = 0.0f;
static volatile float s_max_speed  = 0.0f;   // m/s
static double   s_moving_ms   = 0.0;         // time spent moving (avg denominator)
static uint32_t s_last_svc_ms = 0;
static uint32_t s_step_times[6];             // recent step timestamps (cadence)
static int      s_st_head = 0;
static int      s_st_n    = 0;

static bool try_init(uint8_t addr)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (!bus) {
        return false;
    }
    if (qmi8658_init(&s_imu, bus, addr) != ESP_OK) {
        return false;
    }
    qmi8658_enable_sensors(&s_imu, QMI8658_DISABLE_ALL);
    qmi8658_set_accel_range(&s_imu, QMI8658_ACCEL_RANGE_4G);
    qmi8658_set_accel_odr(&s_imu, QMI8658_ACCEL_ODR_62_5HZ);
    qmi8658_enable_accel(&s_imu, true);
    qmi8658_set_accel_unit_mg(&s_imu, true);
    return true;
}

esp_err_t fpw_imu_init(void)
{
    if (bsp_i2c_init() != ESP_OK) {
        return ESP_FAIL;
    }
    // QMI8658 may sit at either address depending on the SA0 strap.
    s_ready = try_init(QMI8658_ADDRESS_HIGH) || try_init(QMI8658_ADDRESS_LOW);
    if (!s_ready) {
        ESP_LOGE(TAG, "QMI8658 not found on I2C");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "QMI8658 ready");
    return ESP_OK;
}

bool fpw_imu_ready(void) { return s_ready; }

esp_err_t fpw_imu_read_accel_mg(float *ax, float *ay, float *az)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    return qmi8658_read_accel(&s_imu, ax, ay, az);
}

uint32_t fpw_imu_step_count(void) { return s_steps; }
void     fpw_imu_reset_steps(void) { s_steps = 0; }

void fpw_imu_service(void)
{
    if (!s_ready) {
        return;
    }
    float ax, ay, az;
    if (qmi8658_read_accel(&s_imu, &ax, &ay, &az) != ESP_OK) {
        return;
    }
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    s_baseline += (mag - s_baseline) * 0.05f;   // slow gravity/orientation baseline
    float ac = mag - s_baseline;                // dynamic (motion) component

    if (ac > WAKE_MOTION_MG || ac < -WAKE_MOTION_MG) {
        s_motion_event = true;
    }

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);

    // Accumulate "moving time" (denominator for average speed).
    if (s_last_svc_ms != 0) {
        uint32_t dt = now - s_last_svc_ms;
        if (dt < 500 && (now - s_last_step_ms) < 2500) {
            s_moving_ms += dt;
        }
    }
    s_last_svc_ms = now;

    if (ac > STEP_PEAK_MG && s_armed && (now - s_last_step_ms) > STEP_REFRACTORY_MS) {
        s_steps++;
        s_last_step_ms = now;
        s_armed = false;
        s_distance_m += STRIDE_M;

        // Instantaneous speed over the span of the last few steps.
        s_step_times[s_st_head] = now;
        s_st_head = (s_st_head + 1) % 6;
        if (s_st_n < 6) {
            s_st_n++;
        }
        if (s_st_n >= 2) {
            int oldest = (s_st_head - s_st_n + 6) % 6;
            uint32_t span = now - s_step_times[oldest];
            if (span > 0) {
                float inst = ((s_st_n - 1) * STRIDE_M) / (span / 1000.0f);
                if (inst < MAX_PLAUSIBLE_MPS && inst > s_max_speed) {
                    s_max_speed = inst;
                }
            }
        }
    } else if (ac < STEP_REARM_MG) {
        s_armed = true;
    }
}

float fpw_imu_distance_m(void) { return s_distance_m; }
float fpw_imu_max_speed_mps(void) { return s_max_speed; }

float fpw_imu_avg_speed_mps(void)
{
    double secs = s_moving_ms / 1000.0;
    return (secs > 1.0) ? (float)(s_distance_m / secs) : 0.0f;
}

void fpw_imu_reset_stats(void)
{
    s_steps = 0;
    s_distance_m = 0.0f;
    s_max_speed = 0.0f;
    s_moving_ms = 0.0;
    s_st_head = 0;
    s_st_n = 0;
    s_last_step_ms = 0;
}

bool fpw_imu_consume_motion(void)
{
    bool m = s_motion_event;
    s_motion_event = false;
    return m;
}
