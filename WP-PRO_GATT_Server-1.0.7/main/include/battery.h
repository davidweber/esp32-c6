#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
    #endif

    void battery_adc_init(void);
    int  battery_read_voltage_mv(void);
    void battery_update_peukert_factor(float current_a);
    uint8_t battery_get_percentage(void);

    #ifdef __cplusplus
}
#endif
