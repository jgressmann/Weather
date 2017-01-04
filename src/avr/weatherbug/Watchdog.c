#include "Watchdog.h"
#include "../../Misc.h"
#include <avr/io.h>
#include <avr/interrupt.h>


static volatile WDT_callback_t s_DataReceivedCallback NOINIT;


ISR(WDT_vect) {
    s_DataReceivedCallback();
}

void
WDT_SetCallback(WDT_callback_t callback) {
    s_DataReceivedCallback = callback;
}

void
WDT_On(
    unsigned char reset,
    unsigned char interrupt,
    unsigned char prescaler) {

    wdt_reset();
    MCUSR &= ~_BV(WDRF);

    const uint8_t wdtcsr =
            (reset ? _BV(WDE) : 0) |
            (interrupt ? _BV(WDIE) : 0) |
            ((prescaler << 2) & 32) |
            (prescaler & 7);
    /* timed sequence start, 4 cyles */
    WDTCSR |= _BV(WDCE) | _BV(WDE); // for reasons beyond me WDE needs to be set, else no effect = WDT not on
    WDTCSR = wdtcsr;
}

