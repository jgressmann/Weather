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

#ifndef SPI_H
#define SPI_H

#include <avr/cpufunc.h>
#include <avr/io.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* See ATmega328 doc p. 168 */
#define SPI_CLOCK_DIV_4      0
#define SPI_CLOCK_DIV_16     1
#define SPI_CLOCK_DIV_64     2
#define SPI_CLOCK_DIV_128    3

#define SPI_LSB_FIRST       0
#define SPI_MSB_FIRST       1

#define SPI_CLOCK_SPEED_1X  0
#define SPI_CLOCK_SPEED_2X  1

/* See ATmega328 doc p.165 */
#define SPI_MODE_0  0 /* CPOL=0, CPHA=0 Sample (Rising) Setup (Falling) */
#define SPI_MODE_1  1 /* CPOL=0, CPHA=1 Setup (Rising) Sample (Falling) */
#define SPI_MODE_2  2 /* CPOL=1, CPHA=0 Sample (Falling) Setup (Rising) */
#define SPI_MODE_3  3 /* CPOL=1, CPHA=1 Setup (Falling) Sample (Rising) */



#define SPI_Master_Exchange(byte) \
    do { \
        SPDR = byte; /* send */ \
        _NOP(); \
        /* wait to receive */ \
        loop_until_bit_is_set(SPSR, SPIF); \
        byte = SPDR; /* receive */ \
    } while (0)


#ifdef HWRV11


#define SPI_Master_Init(bitOrder, clockDivider, clock, mode) \
    do { \
        /* Set MISO as input, see ATmega328 manual p. 3 for pins */ \
        DDRB &= 0xEF; \
        DDRB |= _BV(DDB5) /* SCK */ | _BV(DDB3) /* MOSI */ | _BV(DDB0) /* SS */; \
        /* Setup SPI */ \
        SPCR = \
            _BV(SPE) | \
            ((bitOrder == SPI_LSB_FIRST) ? _BV(DORD) : 0) | \
            _BV(MSTR) | \
            (mode << 2) | \
            clockDivider; \
        SPSR = (clock == SPI_CLOCK_SPEED_1X ? 0 : _BV(SPI2X)); \
    } while (0)

#define SPI_Master_Start_Transmission() \
    PORTB &= ~_BV(DDB0) /* pull SS low */

#define SPI_Master_End_Transmission() \
    PORTB |= _BV(DDB0) /* pull SS high */



#else /* !defined(HWRV11) */

#define SPI_Master_Init(bitOrder, clockDivider, clock, mode) \
    do { \
        /* Set MOSI, SCK, SS output, all others input, see ATmega328 manual p. 3 for pins */ \
        DDRB &= 0xC3; \
        DDRB |= _BV(DDB5) /* SCK */ | _BV(DDB3) /* MOSI */ | _BV(DDB2) /* SS */; \
        /* Setup SPI */ \
        SPCR = \
            _BV(SPE) | \
            ((bitOrder == SPI_LSB_FIRST) ? _BV(DORD) : 0) | \
            _BV(MSTR) | \
            (mode << 2) | \
            clockDivider; \
        SPSR = (clock == SPI_CLOCK_SPEED_1X ? 0 : _BV(SPI2X)); \
    } while (0)

#define SPI_Master_Start_Transmission() \
    PORTB &= ~_BV(DDB2) /* pull SS low */

#define SPI_Master_End_Transmission() \
    PORTB |= _BV(DDB2) /* pull SS high */


#endif

#ifdef __cplusplus
}
#endif

#endif /* SPI_H */
