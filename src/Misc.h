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

#ifndef MISC_H
#define MISC_H

#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)

#define JOIN2(x, y) x ## y
#define JOIN(x, y) JOIN2(x, y)

#ifdef NDEBUG
#   define NOINIT __attribute__ ((section (".noinit")))
#else
#   define NOINIT
#endif

#define ArduinoDeclare() \
    /* Declared weak in Arduino.h to allow user redefinitions. */ \
    int atexit(void (* /*func*/ )()) { return 0; } \
    /* Weak empty variant initialization function. */ \
    /* May be redefined by variant files. */ \
    void initVariant() __attribute__((weak)); \
    void initVariant() { }


#define ArduinoInit() \
    do { \
        init(); \
        initVariant(); \
    } while (0);

#define IsInWindowT(type, currentWindow, windowSize, value) ((type)(((type)(currentWindow))-((type)(value))) < ((type)(windowSize)))
#define IsInWindow8(currentWindow, windowSize, value) IsInWindowT(uint8_t, currentWindow, windowSize, value)
#define IsInWindow16(currentWindow, windowSize, value) IsInWindowT(uint16_t, currentWindow, windowSize, value)
#define IsInWindow32(currentWindow, windowSize, value) IsInWindowT(uint32_t, currentWindow, windowSize, value)

#ifndef _countof
#   define _countof(x) (sizeof(x)/sizeof((x)[0]))
#endif

#endif /* MISC_H */
