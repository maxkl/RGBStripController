
#include "rgb.h"

#include <msp430.h>
#include <stdbool.h>

// The LED brightness does not scale linearly with the PWM duty cycle,
//   so we map brightness values to PWM duty cycles using a pre-calculated
//   quadratic function in the form a*x^2
static const uint16_t RGB_PWM_LUT[] = {
#include "RGB_PWM_LUT.txt"
};

static bool rgb_enabled = false;

void rgb_init() {
    // RGB LEDs off initially
    P2DIR |= RGB_LED_R | RGB_LED_G | RGB_LED_B;
    P2OUT &= ~(RGB_LED_R | RGB_LED_G | RGB_LED_B);
    P2SEL &= ~(RGB_LED_R | RGB_LED_G | RGB_LED_B);

    // Initialize Timer_A0 for PWM generation
    // SMCLK (16 MHz), stopped
    TA0CTL = TASSEL_2;
    TA0CCTL0 = 0;
    TA0CCTL1 = OUTMOD_7;
    TA0CCTL2 = 0;
    // PWM period
    TA0CCR0 = RGB_PWM_PERIOD - 1;
    // Initial duty cycle
    TA0CCR1 = 0;

    // Initialize Timer_A1 for PWM generation
    // SMCLK (16 MHz), stopped
    TA1CTL = TASSEL_2;
    TA1CCTL0 = 0;
    TA1CCTL1 = OUTMOD_7;
    TA1CCTL2 = OUTMOD_7;
    // PWM period
    TA1CCR0 = RGB_PWM_PERIOD - 1;
    // Initial duty cycle
    TA1CCR1 = 0;
    TA1CCR2 = 0;

    rgb_enabled = false;
}

void rgb_enable() {
    if (rgb_enabled) {
        return;
    }

    // Start the timers
    TA0CTL |= MC_1;
    TA1CTL |= MC_1;

    // Enable PWM outputs
    // TODO: only enable outputs if above threshold?
    P2SEL |= RGB_LED_R | RGB_LED_G | RGB_LED_B;

    rgb_enabled = true;
}

void rgb_disable() {
    if (!rgb_enabled) {
        return;
    }

    // Stop the timers
    TA0CTL &= ~(MC0 | MC1);
    TA1CTL &= ~(MC0 | MC1);

    // Disable PWM outputs
    P2SEL &= ~(RGB_LED_R | RGB_LED_G | RGB_LED_B);

    rgb_enabled = false;
}

void rgb_set(uint16_t r, uint16_t g, uint16_t b) {
    // Correct for non-linear brightness of the LED
    uint16_t duty_cycle_r = RGB_PWM_LUT[r];
    uint16_t duty_cycle_g = RGB_PWM_LUT[g];
    uint16_t duty_cycle_b = RGB_PWM_LUT[b];

    // TODO test duty cycles 0% and 100%
    // // Turn the output off if the duty cycle is below a certain threshold value
    // if (duty_cycle_r < PWM_OFF_THRESHOLD) {
    //     P2SEL &= ~RGB_LED_R;
    // } else {
    //     P2SEL |= RGB_LED_R;
    // }

    // // Turn the output off if the duty cycle is below a certain threshold value
    // if (duty_cycle_g < PWM_OFF_THRESHOLD) {
    //     P2SEL &= ~RGB_LED_G;
    // } else {
    //     P2SEL |= RGB_LED_G;
    // }

    // // Turn the output off if the duty cycle is below a certain threshold value
    // if (duty_cycle_b < PWM_OFF_THRESHOLD) {
    //     P2SEL &= ~RGB_LED_B;
    // } else {
    //     P2SEL |= RGB_LED_B;
    // }

    TA0CCR1 = duty_cycle_r;
    TA1CCR1 = duty_cycle_g;
    TA1CCR2 = duty_cycle_b;
}
