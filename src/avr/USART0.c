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

#include "USART0.h"

#include <util/setbaud.h>
#include <stdint.h>

void
USART0_Init() {
    UBRR0H = UBRRH_VALUE;
    UBRR0L = UBRRL_VALUE;
#if USE_2X
    UCSR0A |= (1 << U2X0);
#else
    UCSR0A &= ~(1 << U2X0);
#endif

    // disable Multi-processor Communication Mode
    UCSR0A &= (uint8_t)~(_BV(MPCM0));
    // z2:0 chars per charactor, we want 8 which is encoded as 3
    UCSR0B = (uint8_t)(
        (0 << RXCIE0 /* interrrupt rx complete */) |
        (0 << TXCIE0 /* interupt tx complete */) |
        (0 << UDRIE0 /* Data Register Empty Interrupt Enable */) |
        (1 << RXEN0 /* receiver enabled */ ) |
        (1 << TXEN0 /* transmitter enabled */) |
        (0 << UCSZ02 /* 8 bit per charactor */ )
        // bit 1:0 are the 8th bit if 9 bits per char is enabled
    );
    UCSR0C = (uint8_t)(
        (0 << UMSEL01 /* async */) |
        (0 << UMSEL00 /* async */) |
        (0 << UPM01 /* no parity */) |
        (0 << UPM00 /* no parity */) |
        (0 << USBS0 /* one stop bit */) |
        (1 << UCSZ01 /* 8 bit per character */) |
        (1 << UCSZ00 /* 8 bit per character */) |
        (0 << UCPOL0/* should be zero for async mode */ )
    );
}

