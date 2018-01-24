
#include <msp430.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <shared/commands.h>
#include <shared/i2c.h>

#ifndef NDEBUG
#define LOGGING
#include "uart.h"
#endif

#include "rgb.h"
#include "color.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*(array)))

#define BRIGHTNESS_MAX 63
#define SPEED_MAX 63

#define MAX_SLAVE_COUNT 16

enum mode {
    MODE_STATIC,
    MODE_ANIMATED
};

#include "colors.h"

static bool is_on = true;

static enum mode selected_mode = MODE_STATIC;
static uint8_t selected_brightness = BRIGHTNESS_MAX;
static uint8_t selected_speed = SPEED_MAX / 8;
static uint8_t selected_color = 0;

static const struct color *animation_colors = NULL;
static uint8_t animation_color_count = 0;
static bool animation_smooth = false;

static uint16_t animation_t = 0;
static uint8_t animation_color_index = 0, animation_next_color_index = 1;

static volatile uint16_t unhandled_animation_steps = 0;

static uint8_t slave_addresses[MAX_SLAVE_COUNT];
static uint8_t slave_count = 0;

static void discover_devices();
static void update_static_color();
static void animate();
static void handle_command(uint8_t command);

static uint16_t interp_1024(uint16_t a, uint16_t b, uint16_t t) {
    return (uint32_t) ((uint32_t) a * (uint32_t) (1023 - t) + (uint32_t) b * t) / (uint32_t) 1023;
}

static void rgb_set_with_brightness(uint16_t r, uint16_t g, uint16_t b, uint8_t brightness) {
    rgb_set(
        (r * brightness) / BRIGHTNESS_MAX,
        (g * brightness) / BRIGHTNESS_MAX,
        (b * brightness) / BRIGHTNESS_MAX
    );
}

int main() {
    // Disable the watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    // Configure the microcontroller to run at 16 MHz
    BCSCTL1 = CALBC1_16MHZ;
    DCOCTL = CALDCO_16MHZ;

    // Configure all pins as outputs
    P1DIR = 0xff;
    P2DIR = 0xff;
    // Configure P2.6 and P2.7 as normal GPIOs (they are configured as XIN and XOUT on reset)
    P2SEL = 0x00;

#ifdef LOGGING
    uart_init();
#endif

    rgb_init();

    i2c_init_master();

    // Enable timer interrupt for counting animation steps
    TA0CTL |= TAIE;

    __enable_interrupt();

    // Give the slaves time to start up (~10 ms)
    __delay_cycles(160000);

    discover_devices();

    // Initialize PWM duty cycles before enabling the outputs
    update_static_color();

    rgb_enable();

    while (1) {
        // Poll I2C slaves for commands
        for (uint8_t i = 0; i < slave_count; i++) {
            // Wait until STOP condition of previous transaction is generated
            // TODO: use "repeated start" feature to speed up polling?
            while (UCB0CTL1 & UCTXSTP);

            UCB0I2CSA = slave_addresses[i];

            // Configure for receiver mode
            UCB0CTL1 &= ~UCTR;
            // Generate START condition
            UCB0CTL1 |= UCTXSTT;

            // Wait until slave acknowledges address
            while (UCB0CTL1 & UCTXSTT);

            // Slave didn't acknowledge address
            if (UCB0STAT & UCNACKIFG) {
                // Generate STOP condition
                UCB0CTL1 |= UCTXSTP;
                continue;
            }

            // Generate STOP condition
            UCB0CTL1 |= UCTXSTP;

            // Wait until data byte received
            while (!(IFG2 & UCB0RXIFG));

            uint8_t command = UCB0RXBUF;

            // The slave may not have a command for us
            if (command != SLAVE_COMMAND_NONE) {
                handle_command(command);
            }
        }

        if (is_on) {
            // Atomically read and clear the number of animation steps we will handle
            __disable_interrupt();
            uint16_t animation_steps = unhandled_animation_steps;
            unhandled_animation_steps = 0;
            __enable_interrupt();

            for (uint16_t i = 0; i < animation_steps; i++) {
                animate();
            }
        }
    }
}

static void broadcast_master_command(uint8_t command) {
    while (UCB0CTL1 & UCTXSTP);

    UCB0I2CSA = 0x00;

    UCB0CTL1 |= UCTR | UCTXSTT;

    while (!(IFG2 & UCB0TXIFG));

    UCB0TXBUF = command;

    while (!(IFG2 & UCB0TXIFG));

    UCB0CTL1 |= UCTXSTP;
}

static void discover_devices() {
    for (uint8_t address = 0x08; address <= 0x77 && slave_count < MAX_SLAVE_COUNT; address++) {
        // Wait until STOP condition of previous transaction is generated
        while (UCB0CTL1 & UCTXSTP);

        UCB0I2CSA = address;

        // Configure for receiver mode
        UCB0CTL1 &= ~UCTR;
        // Generate START condition
        UCB0CTL1 |= UCTXSTT;

        // Wait until slave acknowledges address
        while (UCB0CTL1 & UCTXSTT);

        // There is no slave with that address
        if (UCB0STAT & UCNACKIFG) {
            // Generate STOP condition
            UCB0CTL1 |= UCTXSTP;
            continue;
        }

        // Generate STOP condition
        UCB0CTL1 |= UCTXSTP;

        // Add the slave to our list
        slave_addresses[slave_count] = address;
        slave_count++;
    }
}

static void update_static_color() {
    rgb_set_with_brightness(
        colors_static[selected_color].r,
        colors_static[selected_color].g,
        colors_static[selected_color].b,
        selected_brightness
    );
}

static void animate() {
    switch (selected_mode) {
        case MODE_ANIMATED:
            if (animation_smooth) {
                animation_t += selected_speed + 1;
                if (animation_t > 1023 * 4) {
                    animation_t = 0;

                    animation_color_index = animation_next_color_index;
                    animation_next_color_index = (animation_color_index + 1) % animation_color_count;
                }

                uint16_t t_1024 = animation_t / 4;

                rgb_set_with_brightness(
                    interp_1024(animation_colors[animation_color_index].r, animation_colors[animation_next_color_index].r, t_1024),
                    interp_1024(animation_colors[animation_color_index].g, animation_colors[animation_next_color_index].g, t_1024),
                    interp_1024(animation_colors[animation_color_index].b, animation_colors[animation_next_color_index].b, t_1024),
                    selected_brightness
                );
            } else {
                animation_t += selected_speed * 4 + 1;
                if (animation_t > 1023 * 4) {
                    animation_t = 0;

                    animation_color_index = animation_next_color_index;
                    animation_next_color_index = (animation_color_index + 1) % animation_color_count;
                }

                rgb_set_with_brightness(
                    animation_colors[animation_color_index].r,
                    animation_colors[animation_color_index].g,
                    animation_colors[animation_color_index].b,
                    selected_brightness
                );
            }
            break;
        default:
            break;
    }
}

static void select_color(uint8_t index) {
    selected_mode = MODE_STATIC;
    selected_color = index;

    update_static_color();

    broadcast_master_command(MASTER_COMMAND_ANIMATION_OFF);
}

static void select_animation(const struct color *colors, uint8_t color_count, bool smooth) {
    selected_mode = MODE_ANIMATED;
    animation_colors = colors;
    animation_color_count = color_count;
    animation_smooth = smooth;

    animation_t = 0;
    animation_color_index = 0;
    animation_next_color_index = 1;

    broadcast_master_command(MASTER_COMMAND_ANIMATION_ON);
}

static void set_brightness(uint8_t brightness) {
    selected_brightness = brightness;

    if (selected_mode == MODE_STATIC) {
        update_static_color();
    }
}

static void set_speed(uint8_t speed) {
    selected_speed = speed;
}

static void increment_brightness() {
    if (selected_brightness < BRIGHTNESS_MAX) {
        selected_brightness++;

        if (selected_mode == MODE_STATIC) {
            update_static_color();
        }
    }
}

static void increment_speed() {
    if (selected_speed < SPEED_MAX) {
        selected_speed++;
    }
}

static void decrement_brightness() {
    if (selected_brightness > 1) {
        selected_brightness--;

        if (selected_mode == MODE_STATIC) {
            update_static_color();
        }
    }
}

static void decrement_speed() {
    if (selected_speed > 0) {
        selected_speed--;
    }
}

static void turn_on() {
    if (is_on) {
        return;
    }

    __disable_interrupt();

    is_on = true;

    rgb_enable();

    __enable_interrupt();
}

static void turn_off() {
    if (!is_on) {
        return;
    }

    __disable_interrupt();

    is_on = false;

    rgb_disable();

    __enable_interrupt();
}

static void handle_command(uint8_t command) {
#ifdef LOGGING
    uart_puts("cmd: ");
    uart_puthex(command);
    uart_puts("\r\n");
#endif

    // TODO: broadcast state changes to all slaves:
    //   "mode changed to ANIMATED", "requested visualizer", ...

    switch (command) {
        case SLAVE_COMMAND_OFF: turn_off(); break;
        case SLAVE_COMMAND_ON: turn_on(); break;
        case SLAVE_COMMAND_BRIGHTNESS_DECREMENT: decrement_brightness(); break;
        case SLAVE_COMMAND_BRIGHTNESS_INCREMENT: increment_brightness(); break;
        case SLAVE_COMMAND_SPEED_DECREMENT: decrement_speed(); break;
        case SLAVE_COMMAND_SPEED_INCREMENT: increment_speed(); break;
        default:
            if ((command & 0xf0) == 0x10) {
                switch (command & 0x0f) {
                    case 0: select_animation(colors_flash, ARRAY_SIZE(colors_flash), false); break;
                    case 1: select_animation(colors_strobe, ARRAY_SIZE(colors_strobe), false); break;
                    case 2: select_animation(colors_fade, ARRAY_SIZE(colors_fade), true); break;
                    case 3: select_animation(colors_smooth, ARRAY_SIZE(colors_smooth), true); break;
                }
            } else if ((command & 0xe0) == 0x20) {
                select_color(command & 0x1f);
            } else if ((command & 0xc0) == 0x80) {
                set_brightness(command & 0x3f);
            } else if ((command & 0xc0) == 0xc0) {
                set_speed(command & 0x3f);
            }
    }
}

__attribute__((interrupt(TIMER0_A1_VECTOR)))
void TIMER0_A1_ISR() {
    // Clear TAIFG
    TA0CTL &= ~TAIFG;

    static uint8_t animation_steps_prescaler = 0;

    animation_steps_prescaler++;
    if (animation_steps_prescaler == 128) {
        animation_steps_prescaler = 0;

        unhandled_animation_steps++;
    }
}
