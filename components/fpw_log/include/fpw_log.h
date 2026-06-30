// fpw_log — Kart Telemetry Logger.
//
// Samples the QMI8658 (accel g + gyro deg/s) at a fixed rate to a timestamped
// CSV on the SD card, on its own high-priority task so display sleep/wake has
// zero effect on capture. Toggled by the PWR button. Periodic flush keeps the
// file readable even if power is cut.
//
// Later phase (not implemented): BLE transfer of recorded files. The retrieval
// path is intentionally left as "remove the SD card" for now.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mount the SD card. Call once at boot.
void fpw_log_init(void);

// Capture the resting orientation reference (BOOT button, while stationary):
// averages ~1.5 s of IMU data, stores a gyro bias and a rotation that levels
// the mount. Required before a session can start. Returns false if it could not
// (moving, no IMU, or a session is active). Recalibrate any time when idle.
bool fpw_log_calibrate(void);
bool fpw_log_is_calibrated(void);
bool fpw_log_cal_rejected(void);   // true if a start was refused for being uncalibrated

// Start a session if idle, stop and close cleanly if logging (PWR button).
// A start is rejected unless calibration has been done.
void fpw_log_toggle(void);

bool        fpw_log_active(void);
bool        fpw_log_sd_ok(void);
uint32_t    fpw_log_sample_count(void);
uint32_t    fpw_log_elapsed_ms(void);
const char *fpw_log_filename(void);   // basename of the current/last file

#ifdef __cplusplus
}
#endif
