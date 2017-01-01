
#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>

#define SENSOR_BIT BIT5
#define LED_BIT BIT6
#define LED2_BIT BIT0

volatile bool overflowed;

inline void reset_timer() {
	TAR = 0;
	overflowed = false;
}

int main() {
	WDTCTL = WDTPW | WDTHOLD;

	BCSCTL1 = CALBC1_1MHZ;
	DCOCTL = CALDCO_1MHZ;

	// SMCLK = 1MHz; TACLK = SMCLK / 1 = 1MHz; t = 1 / TACLK = 1us; 16-bit-timer => overflow after 65.536ms
	TACTL = TASSEL_2 | MC_2; // SMCLK, Continuous up
	CCTL0 = CCIE; // Timer interrupt enable
	overflowed = false;

	P1DIR |= LED_BIT | LED2_BIT;
	P1DIR &= ~SENSOR_BIT;
	P1OUT &= ~LED2_BIT;
	P1OUT |= LED_BIT;
	P1IE |= SENSOR_BIT;
	P1IES |= SENSOR_BIT; // falling edge
	P1IFG &= ~SENSOR_BIT;
	P1IE |= SENSOR_BIT;

	// _BIS_SR(CPUOFF | GIE);
	__enable_interrupt();

	while(1) {
		if(overflowed) {
			P1OUT |= LED2_BIT;
		} else {
			P1OUT &= ~LED2_BIT;
		}
	}
}

__attribute__((interrupt(PORT1_VECTOR)))
void PORT1_ISR() {
	uint16_t timer_value = TAR;

	P1IFG &= ~SENSOR_BIT;

	bool falling_edge = P1IES & SENSOR_BIT;
	P1IES ^= SENSOR_BIT;

	// TODO: dump mark and space widths to UART (use USB-UART adapter)
	// IDEA: use large buffer for fast recording, transfer buffer over UART once it is full

	if(falling_edge) {
		P1OUT &= ~LED_BIT;
	} else {
		P1OUT |= LED_BIT;
	}

	reset_timer();
}

__attribute__((interrupt(TIMER0_A0_VECTOR)))
void TIMER0_A0_ISR() {
	overflowed = true;
}
