#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <avr/wdt.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*WDT_callback_t)();

void WDT_SetCallback(WDT_callback_t callback);

void WDT_On(
    unsigned char reset,
    unsigned char interrupt,
    unsigned char prescaler);


#define DEV_SoftReset() \
    do { \
        wdt_enable(WDTO_15MS); \
        for (;;) ; \
    } while(0)


#define WDT_Off() \
    do { \
        wdt_reset(); \
        MCUSR &= ~_BV(WDRF); \
        WDTCSR |= _BV(WDCE); /* timed sequence start, 4 cyles */ \
        WDTCSR = 0; /* off */ \
    } while (0)



#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_H */
