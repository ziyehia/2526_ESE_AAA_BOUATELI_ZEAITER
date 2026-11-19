/*
 * input_analog.h
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#ifndef INC_INPUT_ANALOG_H_
#define INC_INPUT_ANALOG_H_

#include "stm32g4xx_hal.h"

typedef struct {
    uint16_t raw;
    uint16_t voltage_mv;
    int32_t current_ma;
} input_analog_current_t;

typedef struct {
    input_analog_current_t bus;    // Rank 1: PC2 / ADC1_IN8.
    input_analog_current_t motor;  // Rank 2: PA1 / ADC1_IN2 (U_Imes).
} input_analog_measurement_t;

/* Called once at boot with PWM off, after ADC1/DMA/TIM1 configuration.
 * Calibrates ADC1, arms circular DMA, then starts internal TIM1_CH4.
 * One Bus/U sequence per PWM cycle (20 kHz), including with power PWM off.
 * ADC internal calibration does not calibrate the current sensor offsets.
 */
HAL_StatusTypeDef input_analog_init(void);

/* Main context only; nonblocking snapshot of the latest complete Bus/U pair.
 * HAL_BUSY: first pair not yet available; HAL_TIMEOUT: pair older than 10 ms;
 * HAL_ERROR: invalid argument, initialization failure or latched ADC/DMA error.
 * Output is unchanged on failure. No conversion is started by this call.
 * Scaling uses nominal 3.3 V ADC reference, 1.65 V zero, and 50 mV/A.
 */
HAL_StatusTypeDef input_analog_read(input_analog_measurement_t* measurement);

#endif /* INC_INPUT_ANALOG_H_ */
