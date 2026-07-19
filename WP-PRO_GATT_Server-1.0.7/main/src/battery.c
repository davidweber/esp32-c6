#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "BATTERY";

static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

// Battery parameters (can be overridden via battery.h defines)
#ifndef VOLTAGE_DIVIDER_RATIO
#define VOLTAGE_DIVIDER_RATIO   2.0f
#endif

#ifndef BATTERY_FULL_V
#define BATTERY_FULL_V          4.10f
#endif

#ifndef BATTERY_EMPTY_V
#define BATTERY_EMPTY_V         3.00f
#endif

#ifndef PEUKERT_EXPONENT
#define PEUKERT_EXPONENT        1.05f
#endif

#ifndef NOMINAL_DISCHARGE_RATE
#define NOMINAL_DISCHARGE_RATE  0.2f      // C/5 rate
#endif

// Peukert correction factor
static float effective_capacity_factor = 1.0f;

// ADC Configuration
#define BATTERY_ADC_UNIT      ADC_UNIT_1
#define BATTERY_ADC_CHANNEL   ADC_CHANNEL_1     // GPIO3
#define BATTERY_ADC_ATTEN     ADC_ATTEN_DB_12

/**
 * Initialize ADC for battery voltage measurement
 */
void battery_adc_init(void)
{
    // ADC One-shot unit initialization (ESP32-C6 compatible)
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,   // Disable ULP mode
        // .clk_src is omitted → driver uses default
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    // ADC Channel configuration
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &chan_cfg));

    // Calibration using curve fitting (recommended for ESP32-C6)
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATTERY_ADC_UNIT,
        .atten = BATTERY_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle));

    ESP_LOGI(TAG, "Battery ADC initialized on CH3 (GPIO3)");
}

/**
 * Read raw battery voltage in millivolts (after voltage divider)
 */
int battery_read_voltage_mv(void)
{
    int raw = 0;
    int voltage_mv = 0;
    int sum = 0;
    const int samples = 16;

    for (int i = 0; i < samples; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, BATTERY_ADC_CHANNEL, &raw));
        adc_cali_raw_to_voltage(cali_handle, raw, &voltage_mv);
        sum += voltage_mv;
    }

    voltage_mv = sum / samples;

    // Apply voltage divider ratio
    return (int)(voltage_mv * VOLTAGE_DIVIDER_RATIO);
}

/**
 * Update Peukert capacity correction factor based on discharge current
 * @param current_a Discharge current in Amperes
 */
void battery_update_peukert_factor(float current_a)
{
    if (current_a <= 0.0f) {
        effective_capacity_factor = 1.0f;
        return;
    }

    effective_capacity_factor = powf(NOMINAL_DISCHARGE_RATE / current_a, PEUKERT_EXPONENT - 1.0f);

    // Clamp to reasonable range
    if (effective_capacity_factor < 0.6f)  effective_capacity_factor = 0.6f;
    if (effective_capacity_factor > 1.3f)  effective_capacity_factor = 1.3f;
}

/**
 * Get battery percentage (0-100) with Peukert correction
 * Returns value as uint8_t (1 byte)
 */
uint8_t battery_get_percentage(void)
{
    float voltage_v = battery_read_voltage_mv() / 1000.0f;

    if (voltage_v <= BATTERY_EMPTY_V) return 0;
    if (voltage_v >= BATTERY_FULL_V) return 100;

    float base_pct = (voltage_v - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f;
    float corrected_pct = base_pct * effective_capacity_factor;

    if (corrected_pct > 100.0f) corrected_pct = 100.0f;
    if (corrected_pct < 0.0f)   corrected_pct = 0.0f;

    return (uint8_t)corrected_pct;
}
