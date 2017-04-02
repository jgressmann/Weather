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
