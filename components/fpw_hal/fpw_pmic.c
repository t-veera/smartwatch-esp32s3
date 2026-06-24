// fpw_pmic — AXP2101 battery/power readouts.
//
// The BSP already implements AXP2101 init and measurement (XPowersLib-style),
// so this is a thin, stable wrapper that gives the rest of the firmware a
// board-neutral power API.

#include "fpw_pmic.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "esp_log.h"

static const char *TAG = "fpw_pmic";

esp_err_t fpw_pmic_init(void)
{
    esp_err_t err = bsp_power_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_power_init failed: %s", esp_err_to_name(err));
    }
    return err;
}

int  fpw_pmic_battery_percent(void) { return bsp_power_get_battery_percent(); }
int  fpw_pmic_battery_mv(void)      { return bsp_power_get_batt_voltage_mv(); }
bool fpw_pmic_is_charging(void)     { return bsp_power_is_charging(); }
bool fpw_pmic_vbus_present(void)    { return bsp_power_is_vbus_in(); }
