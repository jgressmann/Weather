#include "Timer.h"
#include <avr/interrupt.h>

#ifndef F_CPU
#   error "Define F_CPU"
#endif

#if defined(TMR_TIMER0) || defined(TMR_TIMER1) || defined(TMR_TIMER2)
#   if F_CPU == 16000000L
#       define SHIFT 4
#   elif F_CPU == 8000000L
#       define SHIFT 3
#   else
#       error "Unsupported CPU speed"
#   endif
#endif

#define TimerFunctions(index) \
    static volatile TMR_callback_t JOIN(s_Callback, index) NOINIT; \
    static volatile void* JOIN(s_Ctx, index) NOINIT; \
    ISR(JOIN(TIMER, JOIN(index, _COMPA_vect))) { \
        JOIN(TCNT, index) = 0; /* reset count */ \
        JOIN(s_Callback, index)((void*)JOIN(s_Ctx, index)); \
    } \
    void TMR_NAME(index, SetForCounts)(unsigned ticks, unsigned prescaler, void* ctx, TMR_callback_t callback) { \
        JOIN(TCCR, JOIN(index, B)) = 0; /* disconnect clock */ \
        JOIN(s_Callback, index) = callback; \
        JOIN(s_Ctx, index) = ctx; \
        JOIN(TCCR, JOIN(index, A)) = 0; \
        JOIN(OCR, JOIN(index, A)) = ticks; /* interrupt after xxx ticks */ \
        JOIN(TCNT, index) = 0; /* reset count */ \
        JOIN(TIMSK, index) = 1 << JOIN(JOIN(OCIE, index), A); /* interrupts on */ \
        JOIN(TCCR, JOIN(index, B)) = prescaler; /* connect clock */ \
    } \
    void TMR_NAME(index, SetForMicros)(unsigned micros, void* ctx, TMR_callback_t callback) { \
        TMR_NAME(index, SetForCounts)(micros << SHIFT, 1, ctx, callback); \
    }


#ifdef TMR_TIMER0
TimerFunctions(0)
#endif

#ifdef TMR_TIMER1
TimerFunctions(1)
#endif

#ifdef TMR_TIMER2
TimerFunctions(2)
#endif
