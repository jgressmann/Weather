#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <avr/wdt.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*WDT_callback_t)(void* ctx);
extern void WDT_off();
extern void WDT_ChangePrescaler(unsigned prescaler);
extern void WDT_On(int reset, int interrupt, void* ctx, WDT_callback_t callback);
#define WDT_Reset() wdt_reset();

#ifdef __cplusplus
}
#endif

#endif // WATCHDOG_H
