// fpw_rtc — PCF85063A driver.
//
// Minimal BCD register access over the BSP's shared i2c_master bus. The RTC is
// battery-backed on this board, so a set time survives power loss; the
// oscillator-stop (OS) flag tells us when it has NOT (time is then invalid).

#include "fpw_rtc.h"
#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "fpw_rtc";

#define PCF85063_ADDR   0x51
#define REG_CTRL1       0x00
#define REG_SECONDS     0x04   // block: sec,min,hour,day,wday,month,year
#define CTRL1_STOP_BIT  (1 << 5)
#define SECONDS_OS_FLAG 0x80   // oscillator stopped -> time integrity lost

static i2c_master_dev_handle_t s_dev = NULL;

static inline uint8_t bcd2dec(uint8_t b) { return (uint8_t)((b >> 4) * 10 + (b & 0x0f)); }
static inline uint8_t dec2bcd(uint8_t d) { return (uint8_t)(((d / 10) << 4) | (d % 10)); }

static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t n)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, n, 1000);
}

static esp_err_t reg_write(uint8_t reg, const uint8_t *buf, size_t n)
{
    uint8_t tmp[9];
    if (n > sizeof(tmp) - 1) {
        return ESP_ERR_INVALID_SIZE;
    }
    tmp[0] = reg;
    for (size_t i = 0; i < n; i++) {
        tmp[i + 1] = buf[i];
    }
    return i2c_master_transmit(s_dev, tmp, n + 1, 1000);
}

esp_err_t fpw_rtc_init(void)
{
    if (s_dev) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "i2c init");
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_FAIL, TAG, "no i2c bus");

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCF85063_ADDR,
        .scl_speed_hz    = CONFIG_I2C_MASTER_FREQUENCY,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, &s_dev), TAG, "add dev");

    // Make sure the clock is running (clear STOP); leave 24h mode and cap as-is.
    uint8_t ctrl1 = 0;
    if (reg_read(REG_CTRL1, &ctrl1, 1) == ESP_OK && (ctrl1 & CTRL1_STOP_BIT)) {
        ctrl1 &= (uint8_t)~CTRL1_STOP_BIT;
        reg_write(REG_CTRL1, &ctrl1, 1);
    }
    ESP_LOGI(TAG, "PCF85063 ready at 0x%02X", PCF85063_ADDR);
    return ESP_OK;
}

esp_err_t fpw_rtc_get_time(struct tm *out)
{
    ESP_RETURN_ON_FALSE(s_dev && out, ESP_ERR_INVALID_STATE, TAG, "not ready");
    uint8_t r[7];
    ESP_RETURN_ON_ERROR(reg_read(REG_SECONDS, r, sizeof(r)), TAG, "read");

    out->tm_sec   = bcd2dec(r[0] & 0x7f);
    out->tm_min   = bcd2dec(r[1] & 0x7f);
    out->tm_hour  = bcd2dec(r[2] & 0x3f);          // 24-hour mode
    out->tm_mday  = bcd2dec(r[3] & 0x3f);
    out->tm_wday  = r[4] & 0x07;
    out->tm_mon   = bcd2dec(r[5] & 0x1f) - 1;       // tm_mon is 0-11
    out->tm_year  = bcd2dec(r[6]) + 100;            // 20xx -> years since 1900
    out->tm_isdst = 0;
    return ESP_OK;
}

esp_err_t fpw_rtc_set_time(const struct tm *in)
{
    ESP_RETURN_ON_FALSE(s_dev && in, ESP_ERR_INVALID_STATE, TAG, "not ready");
    int year = in->tm_year + 1900;
    ESP_RETURN_ON_FALSE(year >= 2000 && year <= 2099, ESP_ERR_INVALID_ARG, TAG, "year range");

    uint8_t r[7];
    r[0] = dec2bcd((uint8_t)in->tm_sec) & 0x7f;     // writing seconds clears OS
    r[1] = dec2bcd((uint8_t)in->tm_min) & 0x7f;
    r[2] = dec2bcd((uint8_t)in->tm_hour) & 0x3f;
    r[3] = dec2bcd((uint8_t)in->tm_mday) & 0x3f;
    r[4] = (uint8_t)(in->tm_wday & 0x07);
    r[5] = dec2bcd((uint8_t)(in->tm_mon + 1)) & 0x1f;
    r[6] = dec2bcd((uint8_t)(year - 2000));
    ESP_RETURN_ON_ERROR(reg_write(REG_SECONDS, r, sizeof(r)), TAG, "write");
    return ESP_OK;
}

bool fpw_rtc_time_valid(void)
{
    if (!s_dev) {
        return false;
    }
    uint8_t sec = 0;
    if (reg_read(REG_SECONDS, &sec, 1) != ESP_OK || (sec & SECONDS_OS_FLAG)) {
        return false;
    }
    struct tm t;
    return fpw_rtc_get_time(&t) == ESP_OK && (t.tm_year + 1900) >= 2024;
}
