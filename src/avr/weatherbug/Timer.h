#ifndef TIMER939393939_H
#define TIMER939393939_H

#include "../../Misc.h"
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define TMR_NAME(index, name) JOIN(JOIN(TMR, index), JOIN(_, name))

typedef void (*TMR_callback_t)(void* ctx);

#define TMR_PROTOTYPES(index) \
    /* Sets the timer \
     * \
     * You need to disable interrupts first \
     * Timers are _periodic_, you need to call TMRX_Clear() \
     * in your callback to have a one-shot timer. \
     * \
     * There is no protection against overflow, you need to know \
     * how far each hardware timer can count. \
     *
     * Params micros:   Microseconds before the timer expires and the \
     *      callback is called. \
     * Param ctx:       User defined pointer passed to the callback. \
     * Param callback:  User defined function to call when the timer
     *      elapses. \
     **/ \
    extern void TMR_NAME(index, SetForMicros)(unsigned micros, void* ctx, TMR_callback_t callback); \
    /* Sets the timer
     *
     * Your are responsible to disable interrupts first \
     * Timers are _periodic_, you need to call TMRX_Clear() \
     * in your callback to have a one-shot timer. \
     * \
     * There is no protection against overflow, you need to know \
     * how far each hardware timer can count. \
     *
     * Params ticks:        Ticks before the timer expires and the \
     *      callback is called. \
     * Params prescaler:    See Atmel documentation for values. \
     * Param ctx:           User defined pointer passed to the callback. \
     * Param callback:      User defined function to call when the timer
     *      elapses. \
     **/ \
    extern void TMR_NAME(index, SetForCounts)(unsigned ticks, unsigned prescaler, void* ctx, TMR_callback_t callback)

#ifdef TMR_TIMER0
TMR_PROTOTYPES(0);
#endif

#ifdef TMR_TIMER1
TMR_PROTOTYPES(1);
#endif

#ifdef TMR_TIMER2
TMR_PROTOTYPES(2);
#endif

#undef TMR_PROTOTYPES

#define TMR_Clear(index) \
    do { \
        /* disconnect clock */ \
        JOIN(TCCR, JOIN(index, B)) = 0; \
    } while (0)


/* Clears the timer */
#define TMR0_Clear() TMR_Clear(0)
#define TMR1_Clear() TMR_Clear(1)
#define TMR2_Clear() TMR_Clear(2)



#ifdef __cplusplus
}
#endif

#endif /* TIMER939393939_H */
