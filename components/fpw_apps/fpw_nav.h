// fpw_nav — screen navigation for Field Pocket Watch.
//
// The watchface is home. Swiping moves to one of four apps with an animated
// transition; a swipe back or a long-press returns to the watchface.
//
//        [Steps]              (swipe up)
//           |
//  [Metro]--+--[Recorder]     (swipe left / right)
//           |
//     [Connectivity]          (swipe down)
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Build the watchface + app screens, wire up gestures and load the watchface.
// Call with the LVGL lock held.
void fpw_nav_init(void);

// Toggle the Trackpad app (side button): open it, or return home if showing.
// Thread-safe.
void fpw_nav_toggle_trackpad(void);

#ifdef __cplusplus
}
#endif
