// fpw_power — display sleep/wake state machine.
//
// Step 4 implementation: blank the AMOLED after 10 s of inactivity and wake it
// on a touch or a wrist move. This delivers the observable power behaviour
// (screen dark by default, wakes on tap and wrist twist). True CPU light sleep
// (esp_light_sleep with GPIO38 touch / GPIO21 IMU wake) is a later refinement.

#include "fpw_power.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "fpw_imu.h"

#include "esp_log.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

namespace {
constexpr char TAG[] = "fpw_power";

constexpr uint32_t IDLE_SLEEP_MS = 10000;  // inactivity before the screen blanks
constexpr uint32_t POLL_MS       = 100;
constexpr uint32_t WAKE_DWELL_MS = 1500;   // ignore wrist motion right after sleeping
constexpr uint32_t TOUCH_WAKE_MS = 800;    // recent touch activity threshold

volatile bool s_display_on = true;

esp_pm_lock_handle_t s_usb_awake_lock = nullptr;  // held while USB/charger attached
bool s_usb_lock_held = false;

void enter_sleep()
{
    // Brightness 0 blanks the AMOLED to black while leaving the panel rails (and
    // thus the touch controller and IMU) powered, so both wake sources stay
    // alive. NOTE: bsp_display_sleep() would gate those rails and is avoided
    // here — it also powers down touch/IMU and bricks boot if a reset follows.
    bsp_display_lock(0);
    bsp_display_backlight_off();   // SH8601 brightness -> 0
    bsp_display_unlock();
    fpw_imu_consume_motion();      // drop motion from setting the watch down
    s_display_on = false;
    ESP_LOGI(TAG, "display off");
}

void exit_sleep(const char *cause)
{
    bsp_display_lock(0);
    bsp_display_backlight_on();    // brightness -> 100
    bsp_display_unlock();
    lv_disp_trig_activity(nullptr);
    s_display_on = true;
    ESP_LOGI(TAG, "wake (%s)", cause);
}

void power_task(void *)
{
    vTaskDelay(pdMS_TO_TICKS(2500));   // let the watchface settle after boot
    lv_disp_trig_activity(nullptr);
    fpw_imu_consume_motion();

    uint32_t slept_at_ms = 0;
    while (true) {
        // Stay fully awake while USB/charger is attached (console + USB serial
        // keep working); allow light sleep only on battery.
        bool vbus = bsp_power_is_vbus_in();
        if (vbus && !s_usb_lock_held && s_usb_awake_lock) {
            esp_pm_lock_acquire(s_usb_awake_lock);
            s_usb_lock_held = true;
        } else if (!vbus && s_usb_lock_held) {
            esp_pm_lock_release(s_usb_awake_lock);
            s_usb_lock_held = false;
        }

        if (s_display_on) {
            if (lv_display_get_inactive_time(nullptr) >= IDLE_SLEEP_MS) {
                enter_sleep();
                slept_at_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
            }
        } else {
            uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
            bool touch = lv_display_get_inactive_time(nullptr) < TOUCH_WAKE_MS;
            bool motion = fpw_imu_consume_motion();   // drain every tick
            bool wrist = (now - slept_at_ms > WAKE_DWELL_MS) && motion;
            if (touch) {
                exit_sleep("touch");
            } else if (wrist) {
                exit_sleep("wrist");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}
}  // namespace

extern "C" void fpw_power_init(void)
{
    // Dynamic frequency scaling + automatic light sleep: the CPU drops to
    // 80 MHz and halts between activity when idle, waking on the touch/IMU
    // interrupts. Drivers hold PM locks during transfers, and the logger holds
    // a no-light-sleep lock while recording, so capture and the display are
    // unaffected. On battery this is the big idle-drain saver; while USB is
    // attached the console keeps it awake.
    esp_pm_config_t pm = {};
    pm.max_freq_mhz = 240;
    pm.min_freq_mhz = 80;
    pm.light_sleep_enable = false;   // DFS only — keep USB-Serial/JTAG alive
    esp_err_t err = esp_pm_configure(&pm);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_pm_configure: %s", esp_err_to_name(err));
    }

    esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "usb_awake", &s_usb_awake_lock);
    if (s_usb_awake_lock) {
        esp_pm_lock_acquire(s_usb_awake_lock);   // assume USB attached at boot
        s_usb_lock_held = true;
    }

    xTaskCreate(power_task, "power", 4096, nullptr, 4, nullptr);
}

extern "C" bool fpw_power_display_on(void)
{
    return s_display_on;
}
