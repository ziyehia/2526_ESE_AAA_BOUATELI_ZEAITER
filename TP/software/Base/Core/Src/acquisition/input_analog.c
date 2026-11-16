/*
 * input_analog.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#include "acquisition/input_analog.h"
#include "adc.h"

#define INPUT_ANALOG_POLL_TIMEOUT_MS 1U
#define INPUT_ANALOG_ADC_MAX_CODE 4095U
#define INPUT_ANALOG_ADC_REFERENCE_MV 3300U
#define INPUT_ANALOG_SENSOR_ZERO_MV 1650U
#define INPUT_ANALOG_SENSITIVITY_MV_PER_A 50U
#define MILLIAMPS_PER_AMP 1000U

/* Main-context state: no access from an interrupt or DMA. */
static uint8_t adc_ready;

HAL_StatusTypeDef input_analog_init(void)
{
    adc_ready = 0U;
    // Poll each rank independently; Auto Wait prevents unread data loss.
    if (hadc1.Init.Resolution != ADC_RESOLUTION_12B
        || hadc1.Init.ScanConvMode != ADC_SCAN_ENABLE
        || hadc1.Init.NbrOfConversion != 2U
        || hadc1.Init.EOCSelection != ADC_EOC_SINGLE_CONV
        || hadc1.Init.LowPowerAutoWait != ENABLE
        || hadc1.Init.ContinuousConvMode != DISABLE
        || hadc1.Init.DiscontinuousConvMode != DISABLE
        || hadc1.Init.ExternalTrigConv != ADC_SOFTWARE_START) {
        return HAL_ERROR;
    }
    const HAL_StatusTypeDef status = HAL_ADCEx_Calibration_Start(&hadc1,
                                                              ADC_SINGLE_ENDED);
    if (status == HAL_OK) {
        adc_ready = 1U;
    }
    return status;
}

static input_analog_current_t input_analog_convert(uint32_t raw)
{
    input_analog_current_t result;
    result.raw = (uint16_t)raw;
    result.voltage_mv = (uint16_t)((raw * INPUT_ANALOG_ADC_REFERENCE_MV
        + INPUT_ANALOG_ADC_MAX_CODE / 2U) / INPUT_ANALOG_ADC_MAX_CODE);

    // Keep fractional ADC millivolts until the final conversion into mA.
    // Signed arithmetic preserves negative current; 64 bits avoid overflow.
    const int64_t difference = (int64_t)raw * INPUT_ANALOG_ADC_REFERENCE_MV
        - (int64_t)INPUT_ANALOG_SENSOR_ZERO_MV * INPUT_ANALOG_ADC_MAX_CODE;
    result.current_ma = (int32_t)(difference * MILLIAMPS_PER_AMP
        / (INPUT_ANALOG_ADC_MAX_CODE * INPUT_ANALOG_SENSITIVITY_MV_PER_A));
    return result;
}

HAL_StatusTypeDef input_analog_read(input_analog_measurement_t* measurement)
{
    if (measurement == NULL || adc_ready == 0U) {
        return HAL_ERROR;
    }

    uint32_t bus_raw = 0U;
    uint32_t motor_raw = 0U;
    HAL_StatusTypeDef status = HAL_ADC_Start(&hadc1);
    if (status == HAL_OK) {
        status = HAL_ADC_PollForConversion(&hadc1, INPUT_ANALOG_POLL_TIMEOUT_MS);
        if (status == HAL_OK) {
            bus_raw = HAL_ADC_GetValue(&hadc1);
            // Reading DR releases Auto Wait and lets rank 2 run.
            status = HAL_ADC_PollForConversion(&hadc1, INPUT_ANALOG_POLL_TIMEOUT_MS);
            if (status == HAL_OK) {
                motor_raw = HAL_ADC_GetValue(&hadc1);
            }
        }
    }

    // Stop on every path, including timeout, to restart at rank 1 next time.
    const HAL_StatusTypeDef stop_status = HAL_ADC_Stop(&hadc1);
    if (status != HAL_OK) {
        return status;
    }
    if (stop_status != HAL_OK) {
        return stop_status;
    }
    if (bus_raw > INPUT_ANALOG_ADC_MAX_CODE || motor_raw > INPUT_ANALOG_ADC_MAX_CODE) {
        return HAL_ERROR;
    }

    input_analog_measurement_t result;
    result.bus = input_analog_convert(bus_raw);
    result.motor = input_analog_convert(motor_raw);
    *measurement = result;
    return HAL_OK;
}
