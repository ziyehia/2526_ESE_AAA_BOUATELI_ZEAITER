/*
 * motor.h
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#ifndef INC_MOTOR_CONTROL_MOTOR_H_
#define INC_MOTOR_CONTROL_MOTOR_H_

#include "tim.h"
#include "user_interface/shell.h"

int motor_init(void);

/* Clamp to TIM1 ARR; set CCR1=value and CCR2=ARR-value at the same update. */
uint32_t motor_set_ccr(uint32_t value);
int set_motor_ccr(h_shell_t* h_shell, int argc, char** argv);

#endif /* INC_MOTOR_CONTROL_MOTOR_H_ */
