
#include <msp430.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*(array)))

#define TIME_TOLERANCE 0.5f
#define TIME_MIN(expected) ((uint16_t) ((expected) * (1.0f - TIME_TOLERANCE)))
#define TIME_MAX(expected) ((uint16_t) ((expected) * (1.0f + TIME_TOLERANCE)))

#ifndef NDEBUG
#define LOGGING
#endif

#define PWM_OFF_THRESHOLD 2

#define BRIGHTNESS_MAX 63
#define SPEED_MAX 63

#define NEC_ADDRESS 0x00ef

#define SENSOR_BIT BIT1
#define UART_TXD BIT2

#define NEC_START_PULSE_UNITS 16
#define NEC_START_PAUSE_UNITS 8
#define NEC_REPEAT_PAUSE_UNITS 4
#define NEC_BIT_PULSE_UNITS 1
#define NEC_BIT_0_PAUSE_UNITS 1
#define NEC_BIT_1_PAUSE_UNITS 3

#define NEC_START_PULSE_LENGTH ((uint16_t) (562.5f * 16))
#define NEC_START_PAUSE_LENGTH ((uint16_t) (562.5f * 8))
#define NEC_REPEAT_PAUSE_LENGTH ((uint16_t) (562.5f * 4))
#define NEC_BIT_PULSE_LENGTH ((uint16_t) (562.5f * 1))
#define NEC_BIT_0_PAUSE_LENGTH ((uint16_t) (562.5f * 1))
#define NEC_BIT_1_PAUSE_LENGTH ((uint16_t) (562.5f * 3))
#define NEC_BIT_PAUSE_LENGTH_MID ((NEC_BIT_0_PAUSE_LENGTH + NEC_BIT_1_PAUSE_LENGTH) / 2)
#define NEC_START_REPEAT_PAUSE_LENGTH_MID ((NEC_START_PAUSE_LENGTH + NEC_REPEAT_PAUSE_LENGTH) / 2)

struct color {
    uint16_t r;
    uint16_t g;
    uint16_t b;
};

enum mode {
    MODE_STATIC,
    MODE_ANIMATED
};

enum nec_state {
    NEC_STATE_IDLE,
    NEC_STATE_START,
    NEC_STATE_BIT_PAUSE,
    NEC_STATE_BIT_PULSE
};

static const uint16_t PWM_LUT[] = {
#include "PWM_LUT.txt"
};

static const struct color colors_static[] = {
    // Shades of red   Shades of green    Shades of blue     White
    { 1023, 0, 0 },    { 0, 1023, 0 },    { 0, 0, 1023 },    { 1023, 1023, 1023 },
    { 1023, 256, 0 },  { 0, 1023, 256 },  { 256, 0, 1023 },
    { 1023, 512, 0 },  { 0, 1023, 512 },  { 512, 0, 1023 },
    { 1023, 768, 0 },  { 0, 1023, 768 },  { 768, 0, 1023 },
    { 1023, 1023, 0 }, { 0, 1023, 1023 }, { 1023, 0, 1023 }
};

static const struct color colors_flash[] = {
    { 1023, 0, 0 },
    { 0, 1023, 0 },
    { 0, 0, 1023 }
};

static const struct color colors_strobe[] = {
    { 1023, 0, 0 },
    { 0, 1023, 0 },
    { 0, 0, 1023 },
    { 1023, 1023, 0 },
    { 0, 1023, 1023 },
    { 1023, 0, 1023 },
    { 1023, 1023, 1023 }
};

static const struct color colors_fade[] = {
    { 1023, 0, 0 },
    { 0, 0, 0 },
    { 0, 1023, 0 },
    { 0, 0, 0 },
    { 0, 0, 1023 },
    { 0, 0, 0 }
};

static const struct color colors_smooth[] = {
    { 0, 1023, 0 },
    { 1023, 0, 0 },
    { 0, 0, 1023 },
    { 0, 1023, 0 },
    { 1023, 1023, 1023 }
};

static uint16_t buffered_TA1CCR0 = 0, buffered_TA1CCR1 = 0, buffered_TA1CCR2 = 0;

static bool is_on = true;

static enum mode selected_mode = MODE_STATIC;
static uint8_t selected_brightness = BRIGHTNESS_MAX;
static uint8_t selected_speed = SPEED_MAX / 8;
static uint8_t selected_color = 0;

static const struct color *animation_colors = NULL;
static uint8_t animation_color_count = 0;
static bool animation_smooth = false;

static volatile uint16_t nec_buffer[66];
static volatile uint16_t nec_buffer_index = 0;
static volatile bool nec_buffer_full = false, nec_received_repeat = false;
static uint16_t nec_last_address;
static uint8_t nec_last_command;

static volatile uint16_t unhandled_animation_steps = 0;

#ifdef LOGGING
static void uart_send(uint8_t data);
static void uart_puts(char *s);
static void uart_puthex(uint16_t n);
#endif

static void process_nec_buffer();
static void animate();

static void handle_command(uint8_t addr, uint8_t cmd, bool repeated);

static uint16_t interp(uint16_t a, uint16_t b, uint16_t t) {
    return (uint32_t) ((uint32_t) a * (uint32_t) (1023 - t) + (uint32_t) b * t) / (uint32_t) 1023;
}

static void rgb_set(uint16_t r, uint16_t g, uint16_t b) {
    // Correct for non-linear brightness of the LED
    buffered_TA1CCR0 = PWM_LUT[r];
    buffered_TA1CCR1 = PWM_LUT[g];
    buffered_TA1CCR2 = PWM_LUT[b];
}

static void rgb_set_with_brightness(uint16_t r, uint16_t g, uint16_t b, uint8_t brightness) {
    rgb_set(
        (r * brightness) / BRIGHTNESS_MAX,
        (g * brightness) / BRIGHTNESS_MAX,
        (b * brightness) / BRIGHTNESS_MAX
    );
}

static void update_static_color() {
    rgb_set_with_brightness(
        colors_static[selected_color].r,
        colors_static[selected_color].g,
        colors_static[selected_color].b,
        selected_brightness
    );
}

int main() {
    // Disable the watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    // Configure the microcontroller to run at 8 MHz
    BCSCTL1 = CALBC1_8MHZ;
    DCOCTL = CALDCO_8MHZ;

    // Reset all outputs to a known, power-saving state
    P1DIR = 0xff;
    P1OUT = 0x00;
    P1SEL = 0x00;
    P1SEL2 = 0x00;
    P2DIR = 0xff;
    P2OUT = 0x00;
    P2SEL = 0x00;
    P2SEL2 = 0x00;

    // RGB LEDs off initially
    P2DIR |= BIT0 | BIT1 | BIT4;
    P2SEL &= ~(BIT0 | BIT1 | BIT4);

#ifdef LOGGING
    // UART initialisation

    // Initialize UART pins
    P1SEL |= UART_TXD;
    P1SEL2 |= UART_TXD;

    // Put USCI into reset state
    UCA0CTL1 = UCSWRST;
    // 8 bits data, no parity, 1 stop bit
    UCA0CTL0 = UCMODE_0;
    UCA0CTL1 |= UCSSEL_2;
    // Baud rate: 9600 bps @ 8 MHz (http://mspgcc.sourceforge.net/baudrate.html)
    UCA0BR0 = 0x41;
    UCA0BR1 = 0x03;
    UCA0MCTL = UCBRS_2;
    // Release USCI reset
    UCA0CTL1 &= ~UCSWRST;
#endif

    // Initialize Timer_A0
    // SMCLK divided by 8 (1 MHz), 'Continous up' mode, generate interrupts on rollover
    TA0CTL = TASSEL_2 | ID_3 | MC_2 | TAIE;
    // TACLK = 1 MHz; t = 1 / TACLK = 1 us; 16-bit-timer => interrupt every 65.536 ms
    TA0CCTL0 = CAP | CM_3 | SCS | CCIE;

    // Initialize Timer_A1 for PWM generation
    // SMCLK (8 MHz), 'Continous up' mode, generate interrupts on rollover
    TA1CTL = TASSEL_2 | MC_2 | TAIE;
    // Reset outputs when hitting comparator value, we'll set them on rollover in the ISR using 'output only' mode
    TA1CCTL0 = OUTMOD_5 | OUT;
    TA1CCTL1 = OUTMOD_5 | OUT;
    TA1CCTL2 = OUTMOD_5 | OUT;
    // Initialize comparator values to zero
    TA1CCR0 = 0;
    TA1CCR1 = 0;
    TA1CCR2 = 0;

    update_static_color();

    P1DIR &= ~SENSOR_BIT;
    P1SEL |= SENSOR_BIT;

    __enable_interrupt();

#ifdef LOGGING
    uart_puts("Hello!\r\n");
#endif

    while (1) {
        if (nec_buffer_full) {
            process_nec_buffer();

            nec_buffer_full = false;
        }

        if (nec_received_repeat) {
            handle_command(nec_last_address, nec_last_command, true);

            nec_received_repeat = false;
        }

        if (is_on) {
            uint16_t animation_steps = unhandled_animation_steps;
            for (uint16_t i = 0; i < animation_steps; i++) {
                animate();
            }
            __disable_interrupt();
            unhandled_animation_steps -= animation_steps;
            __enable_interrupt();
        }
    }
}

#ifdef LOGGING
static void uart_send(uint8_t data) {
    while (!(IFG2 & UCA0TXIFG));
    UCA0TXBUF = data;
}

static void uart_puts(char *s) {
    while (*s != '\0') {
        uart_send(*s);
        s++;
    }
}

static void uart_puthex(uint16_t n) {
    static const char hex[] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
        'A', 'B', 'C', 'D', 'E', 'F'
    };

    uart_send(hex[n >> 12]);
    uart_send(hex[(n >> 8) & 0xf]);
    uart_send(hex[(n >> 4) & 0xf]);
    uart_send(hex[n & 0xf]);
}
#endif

static void process_nec_buffer() {
    uint8_t bytes[4] = {0};
    uint8_t bit_index = 0;

    for (uint8_t i = 2; i < 66; i++) {
        bool is_pulse = !(i & 1);
        uint16_t length = nec_buffer[i];

        if (is_pulse) {
            if (length < TIME_MIN(NEC_BIT_PULSE_LENGTH) || length > TIME_MAX(NEC_BIT_PULSE_LENGTH)) {
                return;
            }
        } else {
            if (length < TIME_MIN(NEC_BIT_0_PAUSE_LENGTH) || length > TIME_MAX(NEC_BIT_1_PAUSE_LENGTH)) {
                return;
            }

            if (length > NEC_BIT_PAUSE_LENGTH_MID) {
                bytes[bit_index / 8] |= 1 << (bit_index % 8);
            }

            bit_index++;
        }
    }

    if (bytes[2] == (uint8_t) ~bytes[3]) {
        uint16_t address = bytes[0] << 8 | bytes[1];
        uint8_t command = bytes[2];

        handle_command(address, command, false);

        nec_last_address = address;
        nec_last_command = command;
    }
}

static void animate() {
    static uint16_t t = 0;
    static uint8_t i = 0, i_next = 1;

    switch (selected_mode) {
        case MODE_ANIMATED:
            if (animation_smooth) {
                t += selected_speed + 1;
                if (t > 1023 * 4) {
                    t = 0;

                    i = i_next;
                    i_next = (i + 1) % animation_color_count;
                }

                uint16_t t_1024 = t / 4;

                rgb_set_with_brightness(
                    interp(animation_colors[i].r, animation_colors[i_next].r, t_1024),
                    interp(animation_colors[i].g, animation_colors[i_next].g, t_1024),
                    interp(animation_colors[i].b, animation_colors[i_next].b, t_1024),
                    selected_brightness
                );
            } else {
                t += selected_speed * 4 + 1;
                if (t > 1023 * 4) {
                    t = 0;

                    i = i_next;
                    i_next = (i + 1) % animation_color_count;
                }

                rgb_set_with_brightness(
                    animation_colors[i].r,
                    animation_colors[i].g,
                    animation_colors[i].b,
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
}

static void select_animation(const struct color *colors, uint8_t color_count, bool smooth) {
    selected_mode = MODE_ANIMATED;
    animation_colors = colors;
    animation_color_count = color_count;
    animation_smooth = smooth;
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

    TA1CTL |= TAIE;

    __enable_interrupt();
}

static void turn_off() {
    if (!is_on) {
        return;
    }

    __disable_interrupt();

    is_on = false;

    TA1CTL &= ~TAIE;

    P2SEL &= ~BIT0;
    P2SEL &= ~BIT1;
    P2SEL &= ~BIT2;

    __enable_interrupt();
}

static void handle_command(uint8_t address, uint8_t command, bool repeated) {
#ifdef LOGGING
    uart_puts("cmd: ");
    uart_puthex(address);
    uart_puts(", ");
    uart_puthex(command);
    if (repeated) {
        uart_puts(" (R)");
    }
    uart_puts("\r\n");
#endif

    if (address != NEC_ADDRESS) {
        return;
    }

    switch (command) {
        case 0: if (selected_mode == MODE_ANIMATED) { increment_speed(); } else { increment_brightness(); } break;
        case 1: if (selected_mode == MODE_ANIMATED) { decrement_speed(); } else { decrement_brightness(); } break;
        case 2: turn_off(); break;
        case 3: turn_on(); break;
        case 4: select_color(0); break;
        case 5: select_color(1); break;
        case 6: select_color(2); break;
        case 7: select_color(3); break;
        case 8: select_color(4); break;
        case 9: select_color(5); break;
        case 10: select_color(6); break;
        case 11: select_animation(colors_flash, ARRAY_SIZE(colors_flash), false); break;
        case 12: select_color(7); break;
        case 13: select_color(8); break;
        case 14: select_color(9); break;
        case 15: select_animation(colors_strobe, ARRAY_SIZE(colors_strobe), false); break;
        case 16: select_color(10); break;
        case 17: select_color(11); break;
        case 18: select_color(12); break;
        case 19: select_animation(colors_fade, ARRAY_SIZE(colors_fade), true); break;
        case 20: select_color(13); break;
        case 21: select_color(14); break;
        case 22: select_color(15); break;
        case 23: select_animation(colors_smooth, ARRAY_SIZE(colors_smooth), true); break;
    }
}

#define NEC_REPEAT_TIMEOUT_COUNTER_LIMIT 4 // ~260 ms

static volatile uint16_t last_TA0CCR0 = 0;
static volatile uint16_t nec_repeat_timeout_counter = NEC_REPEAT_TIMEOUT_COUNTER_LIMIT;

__attribute__((interrupt(TIMER0_A0_VECTOR)))
void TIMER0_A0_ISR() {
    uint16_t timestamp = TA0CCR0;
    bool is_pulse = TA0CCTL0 & CCI;

    uint16_t length = timestamp - last_TA0CCR0;
    last_TA0CCR0 = timestamp;

    // if (TA0CCTL0 & COV) {
    //     TA0CCTL0 &= ~COV;
    // }

    uint16_t buffer_index = nec_buffer_index;

    if (buffer_index == 0) {
        TA0CTL |= TACLR;
        last_TA0CCR0 = 0;

        if (!is_pulse || (length < TIME_MIN(NEC_START_PULSE_LENGTH) || length > TIME_MAX(NEC_START_PULSE_LENGTH))) {
            return;
        }
    }

    if (buffer_index == 1) {
        if (length < TIME_MIN(NEC_REPEAT_PAUSE_LENGTH) || length > TIME_MAX(NEC_START_PAUSE_LENGTH)) {
            nec_buffer_index = 0;
            return;
        }

        if (length < NEC_START_REPEAT_PAUSE_LENGTH_MID) {
            if (nec_repeat_timeout_counter < NEC_REPEAT_TIMEOUT_COUNTER_LIMIT) {
                nec_received_repeat = true;

                nec_repeat_timeout_counter = 0;
            }

            nec_buffer_index = 0;
            return;
        }

        nec_repeat_timeout_counter = 0;
    }

    if (nec_buffer_full) {
        return;
    }

    nec_buffer[buffer_index] = length;

    buffer_index++;
    if (buffer_index == 66) {
        buffer_index = 0;

        nec_buffer_full = true;
    }

    nec_buffer_index = buffer_index;
}

__attribute__((interrupt(TIMER0_A1_VECTOR)))
void TIMER0_A1_ISR() {
    TA0CTL &= ~TAIFG;

    last_TA0CCR0 = 0;

    nec_buffer_index = 0;

    if (nec_repeat_timeout_counter < NEC_REPEAT_TIMEOUT_COUNTER_LIMIT) {
        nec_repeat_timeout_counter++;
    }
}

__attribute__((interrupt(TIMER1_A1_VECTOR)))
void TIMER1_A1_ISR() {
    // Stop the timer so we can safely mess with its registers.
    // We also clear TAIFG and the timer counter
    TA1CTL = TASSEL_2 | MC_0 | TAIE | TACLR;

    // Switch to output mode 0 to reset the output state to 1
    TA1CCTL0 = OUTMOD_0 | OUT;
    TA1CCTL1 = OUTMOD_0 | OUT;
    TA1CCTL2 = OUTMOD_0 | OUT;
    // Reset the output mode
    TA1CCTL0 = OUTMOD_5 | OUT;
    TA1CCTL1 = OUTMOD_5 | OUT;
    TA1CCTL2 = OUTMOD_5 | OUT;

    // Apply the buffered duty cycles
    TA1CCR0 = buffered_TA1CCR0;
    TA1CCR1 = buffered_TA1CCR1;
    TA1CCR2 = buffered_TA1CCR2;

    // Let the timer run again
    TA1CTL = TASSEL_2 | MC_2 | TAIE;

    // Turn the output off if the duty cycle is below a certain threshold value
    if (buffered_TA1CCR0 < PWM_OFF_THRESHOLD) {
        P2SEL &= ~BIT0;
    } else {
        P2SEL |= BIT0;
    }

    // Turn the output off if the duty cycle is below a certain threshold value
    if (buffered_TA1CCR1 < PWM_OFF_THRESHOLD) {
        P2SEL &= ~BIT1;
    } else {
        P2SEL |= BIT1;
    }

    // Turn the output off if the duty cycle is below a certain threshold value
    if (buffered_TA1CCR2 < PWM_OFF_THRESHOLD) {
        P2SEL &= ~BIT4;
    } else {
        P2SEL |= BIT4;
    }

    // Increment animation steps
    unhandled_animation_steps++;
}
