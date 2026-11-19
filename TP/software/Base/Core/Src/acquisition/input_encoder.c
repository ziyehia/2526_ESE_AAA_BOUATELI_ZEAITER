/*
 * input_encoder.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#include "acquisition/input_encoder.h"
#include "tim.h"

#define INPUT_ENCODER_COUNTER_MAX 65535U
#define INPUT_ENCODER_MAX_AGE_MS (3U * INPUT_ENCODER_SAMPLE_PERIOD_MS)
#define INPUT_ENCODER_MIN_INTERVAL_MS (INPUT_ENCODER_SAMPLE_PERIOD_MS / 2U)
#define INPUT_ENCODER_MAX_INTERVAL_MS (INPUT_ENCODER_SAMPLE_PERIOD_MS * 3U / 2U)
#define INPUT_ENCODER_MILLISECONDS_PER_MINUTE 60000U
#define INPUT_ENCODER_MILLIRPM_PER_RPM 1000U

/* Written by the TIM2 callback, read by main with interrupts briefly masked.
 * Volatile makes asynchronous accesses visible, but does not protect the pair. */
static volatile uint16_t latest_counter;
static volatile int32_t latest_delta;
static volatile uint32_t latest_tick;
static volatile HAL_StatusTypeDef latest_status;
/* Main sets this startup gate; the timer callback checks it. */
static volatile uint8_t encoder_ready;
/* After initialization this timestamp belongs only to the timer callback. */
static uint32_t previous_tick;

HAL_StatusTypeDef input_encoder_init(void)
{
    encoder_ready = 0U;
    latest_status = HAL_BUSY;
    if (htim3.Init.Prescaler != 0U
        || __HAL_TIM_GET_AUTORELOAD(&htim3) != INPUT_ENCODER_COUNTER_MAX
        || (htim3.Instance->SMCR & TIM_SMCR_SMS) != TIM_ENCODERMODE_TI12
        || htim2.Init.Prescaler != 169U
        || __HAL_TIM_GET_AUTORELOAD(&htim2) != 9999U) {
        return HAL_ERROR;
    }

    __HAL_TIM_SET_COUNTER(&htim3, INPUT_ENCODER_COUNTER_CENTER);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
    HAL_StatusTypeDef status = HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    if (status != HAL_OK) {
        (void)HAL_TIM_Encoder_Stop(&htim3, TIM_CHANNEL_ALL);
        return status;
    }

    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
    previous_tick = HAL_GetTick();
    latest_tick = previous_tick;
    encoder_ready = 1U;
    status = HAL_TIM_Base_Start_IT(&htim2);
    if (status != HAL_OK) {
        encoder_ready = 0U;
        (void)HAL_TIM_Base_Stop_IT(&htim2);
        (void)HAL_TIM_Encoder_Stop(&htim3, TIM_CHANNEL_ALL);
    }
    return status;
}

void input_encoder_sample(void)
{
    if (encoder_ready == 0U) {
        return;
    }
    const uint32_t now = HAL_GetTick();

    /* Keep read/reset adjacent and prevent UART/DMA preemption between them.
     * TIM3 continues to count hardware edges: an edge during these two register
     * accesses can still be lost. IRQ masking does not stop the timer.
     * Quantify this small software-recentering window on the actual hardware. */
    const uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    const uint16_t counter = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
    __HAL_TIM_SET_COUNTER(&htim3, INPUT_ENCODER_COUNTER_CENTER);
    const uint32_t wrapped = __HAL_TIM_GET_FLAG(&htim3, TIM_FLAG_UPDATE);
    __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
    __set_PRIMASK(interrupt_mask);

    const uint32_t elapsed_ms = now - previous_tick;
    previous_tick = now;
    latest_tick = now;
    if (wrapped != RESET || counter == 0U) {
        /* A wrap violates the +/-32767 counts/interval assumption. */
        latest_status = HAL_ERROR;
        return;
    }
    if (elapsed_ms < INPUT_ENCODER_MIN_INTERVAL_MS
        || elapsed_ms > INPUT_ENCODER_MAX_INTERVAL_MS) {
        /* Recenter/rebaseline after a delayed IRQ rather than report a spike. */
        latest_status = HAL_TIMEOUT;
        return;
    }
    latest_counter = counter;
    latest_delta = (int32_t)counter - (int32_t)INPUT_ENCODER_COUNTER_CENTER;
    latest_status = HAL_OK;
}

HAL_StatusTypeDef input_encoder_read(input_encoder_measurement_t* measurement)
{
    if (measurement == NULL) {
        return HAL_ERROR;
    }
    const uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    const uint8_t ready = encoder_ready;
    const HAL_StatusTypeDef status = latest_status;
    const uint16_t counter = latest_counter;
    const int32_t delta = latest_delta;
    const uint32_t age_ms = HAL_GetTick() - latest_tick;
    __set_PRIMASK(interrupt_mask);

    if (ready == 0U) {
        return HAL_ERROR;
    }
    if (status != HAL_OK) {
        if (status == HAL_BUSY && age_ms > INPUT_ENCODER_MAX_AGE_MS) {
            return HAL_TIMEOUT;
        }
        return status;
    }
    if (age_ms > INPUT_ENCODER_MAX_AGE_MS) {
        return HAL_TIMEOUT;
    }

    input_encoder_measurement_t result;
    result.counter = counter;
    result.delta_counts = delta;
    /* n[rpm] = delta * 60000 / (counts_per_rev * period_ms).
     * Use 64 bits before multiplication; avoid float printf on the target. */
    result.speed_mrpm = (int32_t)((int64_t)delta
        * INPUT_ENCODER_MILLISECONDS_PER_MINUTE * INPUT_ENCODER_MILLIRPM_PER_RPM
        / (INPUT_ENCODER_COUNTS_PER_REV * INPUT_ENCODER_SAMPLE_PERIOD_MS));
    *measurement = result;
    return HAL_OK;
}
