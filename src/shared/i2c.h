
#pragma once

#include <stdint.h>
#include <stdbool.h>

#define I2C_SCL BIT6
#define I2C_SDA BIT7

void i2c_init_master();
void i2c_init_slave(uint8_t address, bool general_call);
