// fpw_pmic — AXP2101 power management (thin wrapper over the BSP power API).
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t fpw_pmic_init(void);
int  fpw_pmic_battery_percent(void);
int  fpw_pmic_battery_mv(void);
bool fpw_pmic_is_charging(void);
bool fpw_pmic_vbus_present(void);

#ifdef __cplusplus
}
#endif
