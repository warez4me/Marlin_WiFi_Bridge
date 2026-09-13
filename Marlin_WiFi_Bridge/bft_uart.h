/*
 * Copyright (c) 2026 warez4me
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

#define UART_TXD_INV (BIT(22))
#define UART_OTHER_ERRORS UART_PARITY_ERR_INT_ENA | UART_FRM_ERR_INT_ENA |\
                          UART_DSR_CHG_INT_ENA | UART_CTS_CHG_INT_ENA |  UART_BRK_DET_INT_ENA


#define UART_FIFO_MAX_LEN 128
#define MY_TXFIFO_EMPTY_THRHD (20)

#define APB_FREQ       80000000
#define UART0_BAUDRATE 500000

#define UART_BUF_SIZE      1024
#define UART_BUF_RING_MASK (UART_BUF_SIZE - 1)

char uart_rx_buf[UART_BUF_SIZE] __attribute__((aligned(4)));
volatile uint32_t uart_rx_buf_beg = 0, uart_rx_buf_end = 0;

char uart_tx_buf[UART_BUF_SIZE] __attribute__((aligned(4)));
volatile uint32_t uart_tx_buf_beg = 0, uart_tx_buf_end = 0;

bool uart_on[2] = {false, false};
volatile bool uartWxStop = false;