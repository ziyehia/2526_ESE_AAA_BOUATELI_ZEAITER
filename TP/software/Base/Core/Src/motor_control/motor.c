/*
 * motor.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#include "motor_control/motor.h"
#include <errno.h>
#include <stdlib.h>

uint32_t motor_set_ccr(uint32_t value)
{
	const uint32_t maximum = __HAL_TIM_GET_AUTORELOAD(&htim1);
	if (value > maximum) {
		value = maximum;
	}

	// Suppress preload transfers until both complementary-arm values are ready.
	const uint32_t update_disabled = htim1.Instance->CR1 & TIM_CR1_UDIS;
	SET_BIT(htim1.Instance->CR1, TIM_CR1_UDIS);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, value);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, maximum - value);
	if (update_disabled == 0U) {
		CLEAR_BIT(htim1.Instance->CR1, TIM_CR1_UDIS);
	}
	return value;
}

int motor_init(void){
	// Equal arm duties give zero ideal average bridge voltage at startup.
	motor_set_ccr(__HAL_TIM_GET_AUTORELOAD(&htim1) / 2U);
	// Load both CCR preloads before starting the counter and outputs.
	if (HAL_TIM_GenerateEvent(&htim1, TIM_EVENTSOURCE_UPDATE) != HAL_OK) {
		return HAL_ERROR;
	}

	// Pont Bras U
	if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}

	// Pont Bras V
	if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2) != HAL_OK)
	{
		Error_Handler();
	}

	return 0;
}

static int motor_ccr_error(h_shell_t* h_shell, const char* reason)
{
	const int size = snprintf(h_shell->print_buffer, SHELL_PRINT_BUFFER_SIZE,
		"Error: %s. CCR range: 0-%lu.\r\n", reason,
		(unsigned long)__HAL_TIM_GET_AUTORELOAD(&htim1));
	if (size > 0) {
		const uint16_t length = (size < SHELL_PRINT_BUFFER_SIZE)
			? (uint16_t)size : SHELL_PRINT_BUFFER_SIZE - 1U;
		h_shell->drv.transmit(h_shell->print_buffer, length);
	}
	return HAL_ERROR;
}

int set_motor_ccr(h_shell_t* h_shell, int argc, char** argv){
	if (h_shell == NULL) {
		return HAL_ERROR;
	}

	if(argc != 2 || argv == NULL || argv[1] == NULL){
		return motor_ccr_error(h_shell, "enter one digits-only value");
	}

	if(argv[1][0] == '\0'){
		return motor_ccr_error(h_shell, "enter digits only");
	}

	for(const char* digit = argv[1]; *digit != '\0'; digit++){
		if(*digit < '0' || *digit > '9'){
			return motor_ccr_error(h_shell, "enter digits only");
		}
	}

	// Reject invalid and out-of-range input before touching either PWM channel.
	errno = 0;
	char* end;
	const unsigned long requested = strtoul(argv[1], &end, 10);
	const uint32_t maximum = __HAL_TIM_GET_AUTORELOAD(&htim1);
	if (errno == ERANGE || requested > maximum) {
		return motor_ccr_error(h_shell, "value out of range");
	}
	if (errno != 0 || *end != '\0') {
		return motor_ccr_error(h_shell, "enter digits only");
	}
	const uint32_t applied = motor_set_ccr((uint32_t)requested);
	const int size = snprintf(h_shell->print_buffer, SHELL_PRINT_BUFFER_SIZE,
		"CCR1=%lu CCR2=%lu MAX=%lu\r\n",
		(unsigned long)applied, (unsigned long)(maximum - applied),
		(unsigned long)maximum);
	if (size > 0) {
		const uint16_t length = (size < SHELL_PRINT_BUFFER_SIZE)
			? (uint16_t)size : SHELL_PRINT_BUFFER_SIZE - 1U;
		h_shell->drv.transmit(h_shell->print_buffer, length);
	}
	return HAL_OK;
}

