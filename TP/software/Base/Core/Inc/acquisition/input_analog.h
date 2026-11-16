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

/* Calibrate ADC1 once with PWM off; does not calibrate the sensor offsets. */
HAL_StatusTypeDef input_analog_init(void);

/* Main context only: bounded polling, 1 ms timeout per rank.
 * Output is updated only after both conversions and ADC shutdown succeed.
 * Voltage/current use nominal 3.3 V ADC reference, 1.65 V sensor zero and
 * 50 mV/A sensitivity; calibrate these for accurate physical measurements.
 */
HAL_StatusTypeDef input_analog_read(input_analog_measurement_t* measurement);

#endif /* INC_INPUT_ANALOG_H_ */
