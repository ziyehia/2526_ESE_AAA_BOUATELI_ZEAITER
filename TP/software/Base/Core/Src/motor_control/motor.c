/*
 * motor.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#include "motor_control/motor.h"
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#define MOTOR_RAMP_DURATION_MS 3000U
#define MOTOR_RAMP_INTERVAL_MS 10U

/* Accessed only from application/main context, never from UART interrupts. */
static struct {
    uint8_t pwm_enabled;
    uint8_t ramp_active;
    uint32_t current_ccr;
    uint32_t start_ccr;
    uint32_t target_ccr;
    uint32_t ramp_start_tick;
    uint32_t last_update_tick;
} motor;

static void motor_reply(h_shell_t* h_shell, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const int size = vsnprintf(h_shell->print_buffer, SHELL_PRINT_BUFFER_SIZE,
                              format, arguments);
    va_end(arguments);
    if (size > 0) {
        const uint16_t length = (size < SHELL_PRINT_BUFFER_SIZE)
            ? (uint16_t)size : SHELL_PRINT_BUFFER_SIZE - 1U;
        h_shell->drv.transmit(h_shell->print_buffer, length);
    }
}

static uint32_t motor_apply_ccr(uint32_t value)
{
    const uint32_t maximum = __HAL_TIM_GET_AUTORELOAD(&htim1);
    if (value > maximum) {
        value = maximum;
    }

    // Suppress preload transfers until both arm values are ready.
    const uint32_t update_disabled = htim1.Instance->CR1 & TIM_CR1_UDIS;
    SET_BIT(htim1.Instance->CR1, TIM_CR1_UDIS);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, value);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, maximum - value);
    if (update_disabled == 0U) {
        CLEAR_BIT(htim1.Instance->CR1, TIM_CR1_UDIS);
    }
    motor.current_ccr = value;
    return value;
}

uint32_t motor_set_ccr(uint32_t value)
{
    // A direct command supersedes any pending ramp.
    motor.ramp_active = 0U;
    motor.target_ccr = motor_apply_ccr(value);
    return motor.current_ccr;
}

int motor_stop(void)
{
    int status = HAL_OK;
    motor.pwm_enabled = 0U;
    motor.ramp_active = 0U;
    motor.target_ccr = motor.current_ccr;

    // Gate all TIM1 outputs off before disabling individual channels.
    __HAL_TIM_MOE_DISABLE_UNCONDITIONALLY(&htim1);
    if (HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1) != HAL_OK) {
        status = HAL_ERROR;
    }
    if (HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1) != HAL_OK) {
        status = HAL_ERROR;
    }
    if (HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2) != HAL_OK) {
        status = HAL_ERROR;
    }
    if (HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2) != HAL_OK) {
        status = HAL_ERROR;
    }
    return status;
}

int motor_init(void)
{
    if (motor_stop() != HAL_OK) {
        return HAL_ERROR;
    }
    // Prepare equal duties but leave every PWM output disabled until start.
    motor_set_ccr(__HAL_TIM_GET_AUTORELOAD(&htim1) / 2U);
    return HAL_TIM_GenerateEvent(&htim1, TIM_EVENTSOURCE_UPDATE);
}

int motor_start(void)
{
    // Also makes a repeated start return to 50% and cancels the old ramp.
    if (motor_stop() != HAL_OK) {
        return HAL_ERROR;
    }
    motor_set_ccr(__HAL_TIM_GET_AUTORELOAD(&htim1) / 2U);
    if (HAL_TIM_GenerateEvent(&htim1, TIM_EVENTSOURCE_UPDATE) != HAL_OK) {
        return HAL_ERROR;
    }

    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) {
        goto start_failed;
    }
    if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) {
        goto start_failed;
    }
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2) != HAL_OK) {
        goto start_failed;
    }
    if (HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2) != HAL_OK) {
        goto start_failed;
    }
    motor.pwm_enabled = 1U;
    return HAL_OK;

start_failed:
    // A partial startup must not leave one leg switching on its own.
    (void)motor_stop();
    return HAL_ERROR;
}

void motor_process(void)
{
    if (motor.pwm_enabled == 0U || motor.ramp_active == 0U) {
        return;
    }
    const uint32_t now = HAL_GetTick();
    // Unsigned subtraction also works across the millisecond tick wraparound.
    const uint32_t elapsed = now - motor.ramp_start_tick;
    if (elapsed >= MOTOR_RAMP_DURATION_MS) {
        motor_apply_ccr(motor.target_ccr);
        motor.ramp_active = 0U;
        return;
    }
    if ((uint32_t)(now - motor.last_update_tick) < MOTOR_RAMP_INTERVAL_MS) {
        return;
    }
    motor.last_update_tick = now;

    // Interpolate using elapsed time: fractional increments are not lost.
    // One calculation per call; no catch-up loop if another task ran late.
    uint32_t value;
    if (motor.target_ccr >= motor.start_ccr) {
        const uint32_t distance = motor.target_ccr - motor.start_ccr;
        value = motor.start_ccr
            + (uint32_t)((uint64_t)distance * elapsed / MOTOR_RAMP_DURATION_MS);
    } else {
        const uint32_t distance = motor.start_ccr - motor.target_ccr;
        value = motor.start_ccr
            - (uint32_t)((uint64_t)distance * elapsed / MOTOR_RAMP_DURATION_MS);
    }
    if (value != motor.current_ccr) {
        motor_apply_ccr(value);
    }
}

int motor_ramp_to(uint32_t target)
{
    if (motor.pwm_enabled == 0U || target > __HAL_TIM_GET_AUTORELOAD(&htim1)) {
        return HAL_ERROR;
    }
    motor_process();
    motor.start_ccr = motor.current_ccr;
    motor.target_ccr = target;
    motor.ramp_start_tick = HAL_GetTick();
    motor.last_update_tick = motor.ramp_start_tick;
    motor.ramp_active = (target != motor.current_ccr);
    return HAL_OK;
}

static int motor_ccr_error(h_shell_t* h_shell, const char* reason)
{
    motor_reply(h_shell, "Error: %s. CCR range: 0-%lu.\r\n", reason,
                (unsigned long)__HAL_TIM_GET_AUTORELOAD(&htim1));
    return HAL_ERROR;
}

static int motor_parse_ccr(h_shell_t* h_shell, int argc, char** argv,
                           uint32_t* value)
{
    if (argc != 2 || argv == NULL || argv[1] == NULL) {
        return motor_ccr_error(h_shell, "enter one digits-only value");
    }
    if (argv[1][0] == '\0') {
        return motor_ccr_error(h_shell, "enter digits only");
    }
    for (const char* digit = argv[1]; *digit != '\0'; digit++) {
        if (*digit < '0' || *digit > '9') {
            return motor_ccr_error(h_shell, "enter digits only");
        }
    }

    // Reject invalid/out-of-range input before modifying the ramp or PWM.
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
    *value = (uint32_t)requested;
    return HAL_OK;
}

int set_motor_ccr(h_shell_t* h_shell, int argc, char** argv)
{
    if (h_shell == NULL) {
        return HAL_ERROR;
    }
    uint32_t value;
    if (motor_parse_ccr(h_shell, argc, argv, &value) != HAL_OK) {
        return HAL_ERROR;
    }
    const uint32_t applied = motor_set_ccr(value);
    motor_reply(h_shell, "CCR1=%lu CCR2=%lu MAX=%lu\r\n",
                (unsigned long)applied,
                (unsigned long)(__HAL_TIM_GET_AUTORELOAD(&htim1) - applied),
                (unsigned long)__HAL_TIM_GET_AUTORELOAD(&htim1));
    return HAL_OK;
}

int motor_start_command(h_shell_t* h_shell, int argc, char** argv)
{
    (void)argv;
    if (argc != 1) {
        motor_reply(h_shell, "Error: usage: start\r\n");
        return HAL_ERROR;
    }
    if (motor_start() != HAL_OK) {
        motor_reply(h_shell, "Error: PWM startup failed; outputs disabled.\r\n");
        return HAL_ERROR;
    }
    motor_reply(h_shell, "PWM started: CCR1=%lu CCR2=%lu.\r\n",
                (unsigned long)motor.current_ccr,
                (unsigned long)(__HAL_TIM_GET_AUTORELOAD(&htim1) - motor.current_ccr));
    return HAL_OK;
}

int motor_stop_command(h_shell_t* h_shell, int argc, char** argv)
{
    (void)argv;
    if (argc != 1) {
        motor_reply(h_shell, "Error: usage: stop\r\n");
        return HAL_ERROR;
    }
    if (motor_stop() != HAL_OK) {
        motor_reply(h_shell, "Error: PWM shutdown failed; main output disabled.\r\n");
        return HAL_ERROR;
    }
    motor_reply(h_shell, "PWM stopped; ramp cancelled.\r\n");
    return HAL_OK;
}

int motor_speed_command(h_shell_t* h_shell, int argc, char** argv)
{
    if (h_shell == NULL) {
        return HAL_ERROR;
    }
    uint32_t value;
    if (motor_parse_ccr(h_shell, argc, argv, &value) != HAL_OK) {
        return HAL_ERROR;
    }
    if (motor_ramp_to(value) != HAL_OK) {
        motor_reply(h_shell, "Error: PWM stopped. Use start first.\r\n");
        return HAL_ERROR;
    }
    motor_reply(h_shell, "Speed target CCR1=%lu; ramp=%lu ms.\r\n",
                (unsigned long)value, (unsigned long)MOTOR_RAMP_DURATION_MS);
    return HAL_OK;
}
