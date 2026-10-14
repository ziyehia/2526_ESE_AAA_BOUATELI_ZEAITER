/*
 * app.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#include "app.h"
#include "user_interface/shell.h"

#define SHELL_RX_QUEUE_SIZE 128U
#define SHELL_TX_QUEUE_SIZE 512U
#define SHELL_BYTES_PER_LOOP 16U

static uint8_t shell_uart2_received_char;
static uint8_t shell_rx_queue[SHELL_RX_QUEUE_SIZE];
static uint8_t shell_tx_queue[SHELL_TX_QUEUE_SIZE];
/* UART callbacks and main context share these indices/flags.
 * Main accesses them with interrupts masked; volatile is not the lock. */
static volatile uint16_t shell_rx_head;
static volatile uint16_t shell_rx_tail;
static volatile uint8_t shell_rx_overflow;
static volatile uint16_t shell_tx_head;
static volatile uint16_t shell_tx_tail;
static volatile uint16_t shell_tx_count;

/* Called with interrupts masked, or from the USART2 callback. */
static void shell_uart2_start_tx(void)
{
    if (shell_tx_count != 0U || shell_tx_head == shell_tx_tail) {
        return;
    }
    /* Send a contiguous chunk; its storage remains reserved until completion. */
    shell_tx_count = (shell_tx_head > shell_tx_tail)
        ? (uint32_t)shell_tx_head - shell_tx_tail : SHELL_TX_QUEUE_SIZE - shell_tx_tail;
    if (HAL_UART_Transmit_IT(&huart2, &shell_tx_queue[shell_tx_tail],
                            shell_tx_count) != HAL_OK) {
        shell_tx_count = 0U; /* Retry from the next application loop. */
    }
}

void init_device(void)
{
    hshell1.drv.transmit = shell_uart2_transmit;
    hshell1.drv.receive = shell_uart2_receive;
    shell_init(&hshell1);
    if (shell_add(&hshell1, "SET_CCR", set_motor_ccr, "Set CCR, range 0..4250") != 0) {
        Error_Handler();
    }

    led_init();
    if (motor_init() != HAL_OK) {
        Error_Handler();
    }
    if (HAL_UART_Receive_IT(&huart2, &shell_uart2_received_char, 1) != HAL_OK) {
        Error_Handler();
    }

    // Later lab steps: button, acquisition and closed-loop control initialization.
}

uint8_t shell_uart2_transmit(const char *pData, uint16_t size)
{
    if (pData == NULL || size == 0U || size > UINT8_MAX) {
        return 0;
    }
    const uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    const uint16_t used = (shell_tx_head + SHELL_TX_QUEUE_SIZE - shell_tx_tail)
        % SHELL_TX_QUEUE_SIZE;
    if (size > SHELL_TX_QUEUE_SIZE - 1U - used) {
        __set_PRIMASK(interrupt_mask);
        return 0; /* A full output queue never blocks the application. */
    }
    for (uint16_t i = 0; i < size; i++) {
        shell_tx_queue[shell_tx_head] = (uint8_t)pData[i];
        shell_tx_head = (shell_tx_head + 1U) % SHELL_TX_QUEUE_SIZE;
    }
    shell_uart2_start_tx();
    __set_PRIMASK(interrupt_mask);
    return (uint8_t)size;
}

uint8_t shell_uart2_receive(char *pData, uint16_t size)
{
    if (pData == NULL || size == 0U) {
        return 0;
    }
    const uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    if (shell_rx_overflow != 0U) {
        shell_rx_tail = shell_rx_head;
        shell_rx_overflow = 0U;
        __set_PRIMASK(interrupt_mask);
        shell_discard_line(&hshell1);
        return 0;
    }
    if (shell_rx_head == shell_rx_tail) {
        __set_PRIMASK(interrupt_mask);
        return 0;
    }
    *pData = (char)shell_rx_queue[shell_rx_tail];
    shell_rx_tail = (shell_rx_tail + 1U) % SHELL_RX_QUEUE_SIZE;
    __set_PRIMASK(interrupt_mask);
    return 1;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2) {
        return;
    }
    const uint16_t next = (shell_rx_head + 1U) % SHELL_RX_QUEUE_SIZE;
    if (next == shell_rx_tail) {
        shell_rx_overflow = 1U;
    } else {
        shell_rx_queue[shell_rx_head] = shell_uart2_received_char;
        shell_rx_head = next;
    }
    if (HAL_UART_Receive_IT(&huart2, &shell_uart2_received_char, 1) != HAL_OK) {
        shell_rx_overflow = 1U;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        shell_tx_tail = (shell_tx_tail + shell_tx_count) % SHELL_TX_QUEUE_SIZE;
        shell_tx_count = 0U;
        shell_uart2_start_tx();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        /* Never execute a command assembled across a lost UART byte. */
        shell_rx_overflow = 1U;
        (void)HAL_UART_Receive_IT(&huart2, &shell_uart2_received_char, 1);
    }
}

void loop(void)
{
    const uint32_t interrupt_mask = __get_PRIMASK();
    __disable_irq();
    shell_uart2_start_tx();
    __set_PRIMASK(interrupt_mask);
    for (unsigned int i = 0; i < SHELL_BYTES_PER_LOOP; i++) {
        shell_run(&hshell1);
    }
}
