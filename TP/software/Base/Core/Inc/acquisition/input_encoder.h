/*
 * input_encoder.h
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#ifndef INC_ACQUISITION_INPUT_ENCODER_H_
#define INC_ACQUISITION_INPUT_ENCODER_H_

#include "stm32g4xx_hal.h"

/* 1024 A-channel periods/revolution, decoded on A and B edges (x4).
 * Confirm 4096 counts with one manual shaft revolution on the actual encoder. */
#define INPUT_ENCODER_PERIODS_PER_REV 1024U
#define INPUT_ENCODER_COUNTS_PER_REV (4U * INPUT_ENCODER_PERIODS_PER_REV)
#define INPUT_ENCODER_SAMPLE_PERIOD_MS 10U
#define INPUT_ENCODER_COUNTER_CENTER 32768U

typedef struct {
    uint16_t counter;        /* Counter captured before recentering. */
    int32_t delta_counts;    /* Signed displacement during the last 10 ms. */
    int32_t speed_mrpm;      /* Signed speed, 0.001 revolution/minute units. */
} input_encoder_measurement_t;

/* Called once after TIM3 (quadrature x4) and TIM2 (100 Hz) configuration.
 * Starts acquisition with TIM3 centered at 32768; no motor PWM is enabled. */
HAL_StatusTypeDef input_encoder_init(void);

/* TIM2 period callback only: captures TIM3 and immediately resets it to 32768.
 * No UART, waiting, or speed conversion in this interrupt path. */
void input_encoder_sample(void);

/* Main context: coherent, nonblocking snapshot, converted to milli-rpm.
 * HAL_BUSY before the first sample; HAL_TIMEOUT for stale/missed sampling;
 * HAL_ERROR for invalid arguments, stopped initialization or a counter wrap.
 * Output remains unchanged on error. Positive direction depends on A/B wiring.
 * Each 10 ms interval must contain fewer than 32768 counts in either direction.
 */
HAL_StatusTypeDef input_encoder_read(input_encoder_measurement_t* measurement);



#endif /* INC_ACQUISITION_INPUT_ENCODER_H_ */
