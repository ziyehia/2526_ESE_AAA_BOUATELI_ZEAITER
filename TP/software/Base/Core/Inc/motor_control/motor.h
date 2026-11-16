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

/* Main-context API: startup prepares neutral duty and keeps outputs off. */
int motor_init(void);
int motor_start(void);
int motor_stop(void);

/* Call frequently from loop(); bounded, nonblocking 3 s ramp, 10 ms interval. */
void motor_process(void);
int motor_ramp_to(uint32_t target);

/* Direct update: cancel the ramp, clamp to ARR, and update both CCRs together. */
uint32_t motor_set_ccr(uint32_t value);
int set_motor_ccr(h_shell_t* h_shell, int argc, char** argv);
int motor_start_command(h_shell_t* h_shell, int argc, char** argv);
int motor_stop_command(h_shell_t* h_shell, int argc, char** argv);
int motor_speed_command(h_shell_t* h_shell, int argc, char** argv);

#endif /* INC_MOTOR_CONTROL_MOTOR_H_ */
