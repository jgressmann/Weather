#ifndef DEBUG_H
#define DEBUG_H

#include <util/delay.h>
#include <stdio.h>
#include "USART0.h"

#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)
#ifdef NDEBUG
#   define DEBUG(...)
#else
#   define DEBUG(...) \
    do { \
        char buf[128]; \
        snprintf(buf, sizeof(buf), "%04d: ", __LINE__); \
        USART0_SendString(buf); \
        snprintf(buf, sizeof(buf), __VA_ARGS__); \
        USART0_SendString(buf); \
        USART0_SendFlush(); \
    } while (0)
#endif


#endif // DEBUG_H
