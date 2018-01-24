
#pragma once

#include <stdint.h>

#define RGB_LED_R BIT6
#define RGB_LED_G BIT1
#define RGB_LED_B BIT4

#define RGB_PWM_PERIOD 1024

void rgb_init();
void rgb_disable();
void rgb_enable();
void rgb_set(uint16_t r, uint16_t g, uint16_t b);
