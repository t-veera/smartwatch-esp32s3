// fpw_rtc — PCF85063A real-time clock over the shared I2C bus.
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t fpw_rtc_init(void);
esp_err_t fpw_rtc_get_time(struct tm *out);
esp_err_t fpw_rtc_set_time(const struct tm *in);

// True once the oscillator-stop flag is clear and the year looks plausible,
// i.e. the clock has been set at least once and has kept running.
bool fpw_rtc_time_valid(void);

#ifdef __cplusplus
}
#endif
