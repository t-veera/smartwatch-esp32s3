// fpw_power — display sleep/wake state machine.
//
// After 10 s of inactivity the AMOLED panel is powered down (true black, panel
// rails gated by the AXP2101). The screen wakes on a touch or a wrist movement.
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Start the power-management task. Call after the display and IMU are up.
void fpw_power_init(void);

// True while the display is powered on.
bool fpw_power_display_on(void);

#ifdef __cplusplus
}
#endif
