
#include <msp430.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <shared/commands.h>
#include <shared/i2c.h>

#define SLAVE_ADDRESS 0x11

#define TIME_TOLERANCE 0.5f
#define TIME_MIN(expected) ((uint16_t) ((expected) * (1.0f - TIME_TOLERANCE)))
#define TIME_MAX(expected) ((uint16_t) ((expected) * (1.0f + TIME_TOLERANCE)))

#define NEC_ADDRESS 0x00ef

#define SENSOR_BIT BIT1

#define NEC_START_PULSE_LENGTH ((uint16_t) (562.5f * 16))
#define NEC_START_PAUSE_LENGTH ((uint16_t) (562.5f * 8))
#define NEC_REPEAT_PAUSE_LENGTH ((uint16_t) (562.5f * 4))
#define NEC_BIT_PULSE_LENGTH ((uint16_t) (562.5f * 1))
#define NEC_BIT_0_PAUSE_LENGTH ((uint16_t) (562.5f * 1))
#define NEC_BIT_1_PAUSE_LENGTH ((uint16_t) (562.5f * 3))
#define NEC_BIT_PAUSE_LENGTH_MID ((NEC_BIT_0_PAUSE_LENGTH + NEC_BIT_1_PAUSE_LENGTH) / 2)
#define NEC_START_REPEAT_PAUSE_LENGTH_MID ((NEC_START_PAUSE_LENGTH + NEC_REPEAT_PAUSE_LENGTH) / 2)

enum nec_state {
    NEC_STATE_IDLE,
    NEC_STATE_START,
    NEC_STATE_BIT_PAUSE,
    NEC_STATE_BIT_PULSE
};

static volatile uint16_t nec_buffer[66];
static volatile uint16_t nec_buffer_index = 0;
static volatile bool nec_buffer_full = false, nec_received_repeat = false;
static uint16_t nec_last_address;
static uint8_t nec_last_command;

static volatile bool master_mode_animated = false;

static void process_nec_buffer();

static void handle_command(uint8_t addr, uint8_t cmd, bool repeated);

int main() {
    // Disable the watchdog timer
    WDTCTL = WDTPW | WDTHOLD;

    // Configure the microcontroller to run at 8 MHz
    BCSCTL1 = CALBC1_8MHZ;
    DCOCTL = CALDCO_8MHZ;

    // Configure all pins as outputs
    P1DIR = 0xff;
    P2DIR = 0xff;
    // Configure P2.6 and P2.7 as normal GPIOs (they are configured as XIN and XOUT on reset)
    P2SEL = 0x00;

    // Initialize Timer_A0
    // SMCLK divided by 8 (1 MHz), 'Continous up' mode, generate interrupts on rollover
    TA0CTL = TASSEL_2 | ID_3 | MC_2 | TAIE;
    // TACLK = 1 MHz; t = 1 / TACLK = 1 us; 16-bit-timer => interrupt every 65.536 ms
    TA0CCTL0 = CAP | CM_3 | SCS | CCIE;

    P1DIR &= ~SENSOR_BIT;
    P1SEL |= SENSOR_BIT;

    i2c_init_slave(SLAVE_ADDRESS, true);

    IE2 |= UCB0RXIE | UCB0TXIE;

    __enable_interrupt();

    while (1) {
        if (nec_buffer_full) {
            process_nec_buffer();

            nec_buffer_full = false;
        }

        if (nec_received_repeat) {
            handle_command(nec_last_address, nec_last_command, true);

            nec_received_repeat = false;
        }
    }
}

#define SLAVE_COMMAND_QUEUE_SIZE 16
static volatile uint8_t slave_command_queue[SLAVE_COMMAND_QUEUE_SIZE];
static volatile uint8_t slave_command_queue_front = 0;
static volatile uint8_t slave_command_queue_back = 0;

static void enqueue_slave_command(uint8_t command) {
    __istate_t s = __get_interrupt_state();
    __disable_interrupt();

    uint8_t old_back = slave_command_queue_back;
    uint8_t new_back = (old_back + 1) & (SLAVE_COMMAND_QUEUE_SIZE - 1);
    if (new_back != slave_command_queue_front) {
        slave_command_queue[old_back] = command;
        slave_command_queue_back = new_back;
    }

    __set_interrupt_state(s);
}

static uint8_t dequeue_slave_command() {
    uint8_t command = SLAVE_COMMAND_NONE;

    __istate_t s = __get_interrupt_state();
    __disable_interrupt();

    uint8_t old_front = slave_command_queue_front;
    uint8_t new_front = (old_front + 1) & (SLAVE_COMMAND_QUEUE_SIZE - 1);
    if (new_front != ((slave_command_queue_back + 1) & (SLAVE_COMMAND_QUEUE_SIZE - 1))) {
        command = slave_command_queue[old_front];
        slave_command_queue_front = new_front;
    }

    __set_interrupt_state(s);

    return command;
}

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

static void handle_command(uint8_t address, uint8_t command, bool repeated) {
    if (address != NEC_ADDRESS) {
        return;
    }

    switch (command) {
        case 0: if (master_mode_animated) { enqueue_slave_command(SLAVE_COMMAND_SPEED_INCREMENT); } else { enqueue_slave_command(SLAVE_COMMAND_BRIGHTNESS_INCREMENT); } break;
        case 1: if (master_mode_animated) { enqueue_slave_command(SLAVE_COMMAND_SPEED_DECREMENT); } else { enqueue_slave_command(SLAVE_COMMAND_BRIGHTNESS_DECREMENT); } break;
        case 2: enqueue_slave_command(SLAVE_COMMAND_OFF); break;
        case 3: enqueue_slave_command(SLAVE_COMMAND_ON); break;
        case 4: enqueue_slave_command(SLAVE_COMMAND_COLOR(0)); break;
        case 5: enqueue_slave_command(SLAVE_COMMAND_COLOR(5)); break;
        case 6: enqueue_slave_command(SLAVE_COMMAND_COLOR(10)); break;
        case 7: enqueue_slave_command(SLAVE_COMMAND_COLOR(15)); break;
        case 8: enqueue_slave_command(SLAVE_COMMAND_COLOR(1)); break;
        case 9: enqueue_slave_command(SLAVE_COMMAND_COLOR(6)); break;
        case 10: enqueue_slave_command(SLAVE_COMMAND_COLOR(11)); break;
        case 11: enqueue_slave_command(SLAVE_COMMAND_ANIMATION(0)); break;
        case 12: enqueue_slave_command(SLAVE_COMMAND_COLOR(2)); break;
        case 13: enqueue_slave_command(SLAVE_COMMAND_COLOR(7)); break;
        case 14: enqueue_slave_command(SLAVE_COMMAND_COLOR(12)); break;
        case 15: enqueue_slave_command(SLAVE_COMMAND_ANIMATION(1)); break;
        case 16: enqueue_slave_command(SLAVE_COMMAND_COLOR(3)); break;
        case 17: enqueue_slave_command(SLAVE_COMMAND_COLOR(8)); break;
        case 18: enqueue_slave_command(SLAVE_COMMAND_COLOR(13)); break;
        case 19: enqueue_slave_command(SLAVE_COMMAND_ANIMATION(2)); break;
        case 20: enqueue_slave_command(SLAVE_COMMAND_COLOR(4)); break;
        case 21: enqueue_slave_command(SLAVE_COMMAND_COLOR(9)); break;
        case 22: enqueue_slave_command(SLAVE_COMMAND_COLOR(14)); break;
        case 23: enqueue_slave_command(SLAVE_COMMAND_ANIMATION(3)); break;
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

__attribute__((interrupt(USCIAB0TX_VECTOR)))
void USCIAB0TX_ISR() {
    if (IFG2 & UCB0RXIFG) {
        uint8_t master_command = UCB0RXBUF;

        switch (master_command) {
            case MASTER_COMMAND_ANIMATION_OFF:
                master_mode_animated = false;
                break;
            case MASTER_COMMAND_ANIMATION_ON:
                master_mode_animated = true;
                break;
        }
    }

    if (IFG2 & UCB0TXIFG) {
        UCB0TXBUF = dequeue_slave_command();
    }
}
