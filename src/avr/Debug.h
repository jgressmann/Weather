/* The MIT License (MIT)
 *
 * Copyright (c) 2016 Jean Gressmann <jean@0x42.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef AVR_DEBUG_H
#define AVR_DEBUG_H

#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)

#include <avr/interrupt.h> /* for SREG */

#ifdef NDEBUG
#   define DEBUG(...)
#   define DEBUG_P(...)
#else /* !defined(NDEBUG) */
#   include <avr/pgmspace.h>
#   include <stdio.h>
#   include <stdlib.h>
#   ifdef __cplusplus
extern "C" {
#   endif
extern FILE* s_FILE_Debug;
#   ifdef __cplusplus
}
#   endif

#   define DEBUG(...) fprintf(s_FILE_Debug, __VA_ARGS__)
/* Thank you, GCC
 * https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
 * */
#   define DEBUG_P(fmt, ...) fprintf_P(s_FILE_Debug, PSTR(fmt), ##__VA_ARGS__)
#endif /* !defined(NDEBUG) */


#define ASSERT_FILE(x, codeToRunOnFail, file) \
    do { \
        if (!(x)) { \
            DEBUG_P("%s(%d): ASSERTION FAILED %s\n", file, __LINE__, STRINGIFY(x)); \
            codeToRunOnFail; \
        } else if (0) { \
            DEBUG_P("%s(%d): OK\n", file, __LINE__); \
        } \
    } while (0)

#define ASSERT(x, codeToRunOnFail) ASSERT_FILE(x, codeToRunOnFail, __FILE__)
#define ASSERT_INTERRUPTS_OFF(codeToRunOnFail, file) ASSERT_FILE(!(SREG & 128), codeToRunOnFail, file)

#endif /* AVR_DEBUG_H */
