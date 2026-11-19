/*
 * input_analog.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

/* Periodic current acquisition: TIM1 OC4REF/TRGO2 -> ADC1 -> circular DMA. */
#include "acquisition/input_analog.h"
#include "adc.h"
#include "tim.h"

#define INPUT_ANALOG_ADC_MAX_CODE 4095U
#define INPUT_ANALOG_ADC_REFERENCE_MV 3300U
#define INPUT_ANALOG_SENSOR_ZERO_MV 1650U
#define INPUT_ANALOG_SENSITIVITY_MV_PER_A 50U
#define MILLIAMPS_PER_AMP 1000U
#define INPUT_ANALOG_CHANNEL_COUNT 2U
#define INPUT_ANALOG_DMA_SAMPLE_COUNT (2U * INPUT_ANALOG_CHANNEL_COUNT)
#define INPUT_ANALOG_MAX_AGE_MS 10U

/* DMA writes this storage independently of CPU execution. Each half holds
 * one complete [Bus, U] sequence; callbacks read the completed half only. */
__ALIGNED(4) static volatile uint16_t dma_samples[INPUT_ANALOG_DMA_SAMPLE_COUNT];

/* DMA/ADC callbacks publish raw data and status to main context.
 * Volatile ensures accesses occur; the critical section in read() protects
 * the complete pair and its metadata, not volatile alone. */
static volatile uint16_t latest_raw[INPUT_ANALOG_CHANNEL_COUNT];
static volatile uint32_t latest_tick;
static volatile uint8_t measurement_available;
static volatile uint8_t acquisition_failed;
static volatile uint8_t adc_ready;

HAL_StatusTypeDef input_analog_init(void)
{
    /* Called once at boot with TIM1 and all converter outputs stopped. */
    adc_ready = 0U;
    measurement_available = 0U;
    acquisition_failed = 0U;
    if (hadc1.Init.Resolution != ADC_RESOLUTION_12B
        || hadc1.Init.ScanConvMode != ADC_SCAN_ENABLE
        || hadc1.Init.NbrOfConversion != INPUT_ANALOG_CHANNEL_COUNT
        || hadc1.Init.LowPowerAutoWait != DISABLE
        || hadc1.Init.ContinuousConvMode != DISABLE
        || hadc1.Init.DiscontinuousConvMode != DISABLE
        || hadc1.Init.ExternalTrigConv != ADC_EXTERNALTRIG_T1_TRGO2
        || hadc1.Init.ExternalTrigConvEdge != ADC_EXTERNALTRIGCONVEDGE_RISING
        || hadc1.Init.DMAContinuousRequests != ENABLE
        || hadc1.DMA_Handle == NULL
        || hadc1.DMA_Handle->Init.Mode != DMA_CIRCULAR
        || hadc1.DMA_Handle->Init.PeriphDataAlignment != DMA_PDATAALIGN_HALFWORD
        || hadc1.DMA_Handle->Init.MemDataAlignment != DMA_MDATAALIGN_HALFWORD) {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status = HAL_ADCEx_Calibration_Start(&hadc1,
                                                         ADC_SINGLE_ENDED);
    if (status != HAL_OK) {
        return status;
    }

    /* HAL accepts a word pointer; DMA alignment above selects 16-bit writes. */
    status = HAL_ADC_Start_DMA(&hadc1, (uint32_t *)(uintptr_t)dma_samples,
                               INPUT_ANALOG_DMA_SAMPLE_COUNT);
    if (status != HAL_OK) {
        (void)HAL_ADC_Stop_DMA(&hadc1);
        return status;
    }

    __HAL_TIM_SET_COUNTER(&htim1, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim1, TIM_FLAG_UPDATE);
    adc_ready = 1U;
    /* CH4 is internal only. Keeping CC4E active also keeps TIM1 counting
     * when HAL PWM_Stop disables the four converter channels later. */
    status = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
    /* Starting a channel sets MOE in HAL; keep power outputs gated at boot. */
    __HAL_TIM_MOE_DISABLE_UNCONDITIONALLY(&htim1);
    if (status != HAL_OK) {
        adc_ready = 0U;
        (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);
        (void)HAL_ADC_Stop_DMA(&hadc1);
    }
    return status;
}

/* At 20 kHz, the completed half remains untouched for the next 50 us.
 * Callbacks perform bounded copies only: no UART, conversion or waiting. */
static void input_analog_publish(uint32_t offset)
{
    if (adc_ready == 0U || acquisition_failed != 0U) {
        return;
    }
    /* If an IRQ was delayed, do not read a half already being rewritten.
     * CNDTR counts down from 4: first half is stable at 2..1, second at 4..3.
     * Check again after copying in case DMA crossed the half boundary. */
    uint32_t remaining = __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
    if ((offset == 0U && (remaining == 0U || remaining > INPUT_ANALOG_CHANNEL_COUNT))
        || (offset != 0U && remaining <= INPUT_ANALOG_CHANNEL_COUNT)) {
        return;
    }
    const uint16_t bus_raw = dma_samples[offset];
    const uint16_t motor_raw = dma_samples[offset + 1U];
    remaining = __HAL_DMA_GET_COUNTER(hadc1.DMA_Handle);
    if ((offset == 0U && (remaining == 0U || remaining > INPUT_ANALOG_CHANNEL_COUNT))
        || (offset != 0U && remaining <= INPUT_ANALOG_CHANNEL_COUNT)) {
        return;
    }
    if (bus_raw > INPUT_ANALOG_ADC_MAX_CODE
        || motor_raw > INPUT_ANALOG_ADC_MAX_CODE) {
        acquisition_failed = 1U;
        return;
    }
    latest_raw[0] = bus_raw;
    latest_raw[1] = motor_raw;
    latest_tick = HAL_GetTick();
    measurement_available = 1U;
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc == &hadc1) {
        input_analog_publish(0U);
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc == &hadc1) {
        input_analog_publish(INPUT_ANALOG_CHANNEL_COUNT);
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc == &hadc1) {
        /* Latch ADC overrun / DMA errors; never report old data as fresh. */
        acquisition_failed = 1U;
    }
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
    if (measurement == NULL) {
        return HAL_ERROR;
    }

    /* Mask callbacks only while copying the published pair and its status.
     * DMA continues writing its separate circular buffer during this copy. */
    const uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    const uint8_t ready = adc_ready;
    const uint8_t failed = acquisition_failed;
    const uint8_t available = measurement_available;
    const uint16_t bus_raw = latest_raw[0];
    const uint16_t motor_raw = latest_raw[1];
    const uint32_t age_ms = HAL_GetTick() - latest_tick;
    __set_PRIMASK(interrupt_mask);

    if (ready == 0U || failed != 0U) {
        return HAL_ERROR;
    }
    if (available == 0U) {
        return HAL_BUSY;
    }
    if (age_ms > INPUT_ANALOG_MAX_AGE_MS) {
        return HAL_TIMEOUT;
    }

    input_analog_measurement_t result;
    result.bus = input_analog_convert(bus_raw);
    result.motor = input_analog_convert(motor_raw);
    *measurement = result;
    return HAL_OK;
}
