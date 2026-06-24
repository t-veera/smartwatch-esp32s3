// Field Pocket Watch — firmware entry point.
//
// Boot sequence (built out across the sprint):
//   Step 1  -> BSP + display/touch bring-up
//   Step 2  -> peripheral init (RTC, PMIC, IMU)
//   Step 3+ -> esp-brookesia start, watchface, apps
//
// For Step 0 this is an empty main that proves the toolchain and
// project scaffolding build cleanly against ESP-IDF.

#include "esp_log.h"

namespace {
constexpr char TAG[] = "fpw";
}  // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Field Pocket Watch — boot");
}
