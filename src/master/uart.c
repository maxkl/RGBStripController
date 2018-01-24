
#include "uart.h"

#include <msp430.h>

void uart_init() {
    // Initialize UART pins
    P1SEL |= UART_RXD | UART_TXD;
    P1SEL2 |= UART_RXD | UART_TXD;

    // Enter reset state
    UCA0CTL1 = UCSWRST;

    // 8 bits data, no parity, 1 stop bit
    UCA0CTL0 = UCMODE_0;
    // SMCLK
    UCA0CTL1 |= UCSSEL_2;
    // Baud rate: 9600 bps @ 16 MHz (http://mspgcc.sourceforge.net/baudrate.html)
    UCA0BR0 = 0x82;
    UCA0BR1 = 0x06;
    UCA0MCTL = UCBRS_5;

    // Release USCI reset
    UCA0CTL1 &= ~UCSWRST;
}

void uart_send(uint8_t data) {
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = data;
}

void uart_puts(char *s) {
    while (*s != '\0') {
        uart_send(*s);
        s++;
    }
}

void uart_puthex(uint16_t n) {
    static const char hex[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F'
    };

    uart_send(hex[n >> 12]);
    uart_send(hex[(n >> 8) & 0xf]);
    uart_send(hex[(n >> 4) & 0xf]);
    uart_send(hex[n & 0xf]);
}
