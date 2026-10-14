/*
 * motor.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#include "motor_control/motor.h"

int motor_init(void){

	// Pont Bras U
	if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}

	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2550); // 50% duty cycle (ARR = 8499)

	// Pont Bras V
	if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2) != HAL_OK)
	{
		Error_Handler();
	}

	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 2550); // 50% duty cycle (ARR = 8499)

	return 0;
}


