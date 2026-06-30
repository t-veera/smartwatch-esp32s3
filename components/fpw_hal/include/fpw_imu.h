// fpw_imu — QMI8658 6-axis IMU: accelerometer + software step counter.
//
// Step 2 uses polled accel reads and a lightweight software pedometer. The
// QMI8658 INT pin (GPIO21, active-low) and its wake-on-motion engine are wired
// up later for wrist-wake (Step 4).
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t fpw_imu_init(void);
bool      fpw_imu_ready(void);
esp_err_t fpw_imu_read_accel_mg(float *ax, float *ay, float *az);

// Combined accel (g) + gyro (deg/s) read for telemetry logging.
esp_err_t fpw_imu_read_all(float *ax, float *ay, float *az,
                           float *gx, float *gy, float *gz);

// Call periodically (~50 Hz) to feed the step detector.
void      fpw_imu_service(void);
uint32_t  fpw_imu_step_count(void);
void      fpw_imu_reset_steps(void);

// Day stats derived from steps. Speeds in m/s, distance in metres.
float     fpw_imu_distance_m(void);
float     fpw_imu_avg_speed_mps(void);   // average over time spent moving
float     fpw_imu_max_speed_mps(void);
void      fpw_imu_reset_stats(void);      // steps, distance, speeds (the reset button)

// Returns true once if "wake-worthy" motion (e.g. a wrist raise) has occurred
// since the last call, then clears the flag. Used for wrist-wake.
bool      fpw_imu_consume_motion(void);

#ifdef __cplusplus
}
#endif
