
#include "i2c.h"

#include <msp430.h>

void i2c_init_master() {
    // Enter reset state
    UCB0CTL1 = UCSWRST;

    // I2C mode, master
    UCB0CTL0 = UCSYNC | UCMODE_3 | UCMST;
    // SMCLK
    UCB0CTL1 |= UCSSEL_2;
    // Clock prescaler (16MHz / 40 -> 400kHz)
    UCB0BR0 = 160;
    UCB0BR1 = 0;

    P1SEL |= I2C_SCL | I2C_SDA;
    P1SEL2 |= I2C_SCL | I2C_SDA;

    // Exit reset state
    UCB0CTL1 &= ~UCSWRST;
}

void i2c_init_slave(uint8_t address, bool general_call) {
    // Enter reset state
    UCB0CTL1 = UCSWRST;

    // I2C mode
    UCB0CTL0 = UCSYNC | UCMODE_3;

    UCB0I2COA = (general_call ? UCGCEN : 0) | address;

    P1SEL |= I2C_SCL | I2C_SDA;
    P1SEL2 |= I2C_SCL | I2C_SDA;

    // Exit reset state
    UCB0CTL1 &= ~UCSWRST;
}
