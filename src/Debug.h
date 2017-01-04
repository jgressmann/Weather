#ifndef DEBUG_H
#define DEBUG_H

#ifdef AVR
#   include "avr/Debug.h"
#else
#   include <stdio.h>
#   include <assert.h>
#   define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#   define DEBUG_P(...) DEBUG(__VA_ARGS__)
#   define STRINGIFY1(x) #x
#   define STRINGIFY(x) STRINGIFY1(x)
#   define ASSERT_FILE(x, codeToRunOnFail, file) \
        do {\
            if (!(x)) { \
                DEBUG("%s(%d): ASSERTION FAILED %s\n", file, __LINE__, STRINGIFY(x)); \
                codeToRunOnFail; \
            } \
        } while (0)


#   define ASSERT(x, codeToRunOnFail) ASSERT_FILE(x, codeToRunOnFail, __FILE__)
#   define ASSERT_INTERRUPTS_OFF(...)
#endif /* !AVR */



#endif /* DEBUG_H */
