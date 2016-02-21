#include "Watchdog.h"
#include "Debug.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <inttypes.h>

static
void
Nop(void* ctx) {
    (void)ctx;
}

static volatile WDT_callback_t s_Callback = Nop;
static volatile void* s_Ctx;

// Arduino on-board LED is on pin 13 which is PB5 in AVR
ISR(WDT_vect) {
    s_Callback((void*)s_Ctx);
}

#define WDT_Set(x) \
    do { \
        /* Time sequence 4 ticks */ \
        WDTCSR = (1<<WDCE) | (1<<WDE); \
        WDTCSR = x & ~(1<<WDCE); \
    } while (0)

void
WDT_off() {
    uint8_t reg;
    wdt_reset();
    /* Clear WDRF in MCUSR */
    MCUSR &= ~(1<<WDRF);
    reg = WDTCSR & ~((1<<WDE) | (1<<WDIE));
    WDT_Set(reg);
    DEBUG("WDTCSR: %02x\n", WDTCSR);
}

void
WDT_ChangePrescaler(unsigned prescaler) {
    uint8_t reg;
    //char buf[64];
    wdt_reset();
    // filter out
    reg = WDTCSR & ~((1<<WDCE) | (1<<WDE));
    //sprintf(buf, "WDTCSR before %x\n", reg);
    //USART_SendString(buf);
    reg &= ~(32 | 7); // clear previoud prescaler bits
    reg |= (prescaler & 16 << 1) | (prescaler & 7);
    WDT_Set(reg);
    DEBUG("WDTCSR: %02x\n", WDTCSR);
}

void
WDT_On(int reset, int interrupt, void* ctx, WDT_callback_t callback) {
    wdt_reset();
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        s_Callback = callback ? callback : Nop;
        s_Ctx = ctx;
    }
    uint8_t reg = (WDTCSR & ~((1<<WDE) | (1<<WDIE))) | (reset ? (1<<WDE) : 0) | (interrupt ? (1<<WDIE) : 0);
    WDT_Set(reg);
    DEBUG("WDTCSR: %02x\n", WDTCSR);
}
