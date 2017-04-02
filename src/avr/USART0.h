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

#ifndef USART0_H
#define USART0_H

#include <avr/io.h>
#include <util/delay.h>

#ifdef __cplusplus
extern "C" {
#endif

#define USART0_WaitForActiveSend() loop_until_bit_is_set(UCSR0A, UDRE0)
#define USART0_SendFlush() \
    do { \
        USART0_WaitForActiveSend(); \
        /* 200 us works for Arduino to ensure data hits receiver @ 115200 */ \
        /* 2400 us works for Arduino to ensure data hits receiver @ 9600 */ \
        switch ((long)BAUD) { \
        default: case 9600L: _delay_us(2400); break; \
        case 38400L: _delay_us(800); break; \
        case 57600L: _delay_us(400); break; \
        case 115200L: _delay_us(200); break; \
        } \
    } while (0)

#define USART0_SendByte(byte) \
    do { \
        /* Wait for empty transmit buffer */ \
        USART0_WaitForActiveSend(); \
        /* Put data into buffer, sends the data */ \
        UDR0 = byte; \
    } while (0)

#define USART0_SendString(str) \
    do { \
        const char* x = str; \
        while (*x) { \
            USART0_SendByte(*x++); \
        } \
    } while (0)

#define USART0_SendString_P(str) \
    do { \
        const char* x = str; \
        for (uint8_t byte; (byte = pgm_read_byte(x)) != 0; ++x) { \
            USART0_SendByte(byte); \
        } \
    } while (0)




#define USART0_HasReceivedByte() (UCSR0A & _BV(RXC0))
#define USART0_FetchReceivedByte() UDR0
extern void USART0_Init();
#define USART0_Uninit() UCSR0B = 0

#ifdef __cplusplus
}
#endif

#endif /* USART0_H */
