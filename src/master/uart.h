
#pragma once

#include <stdint.h>

#define UART_RXD BIT1
#define UART_TXD BIT2

void uart_init();
void uart_send(uint8_t data);
void uart_puts(char *s);
void uart_puthex(uint16_t n);
