#include "USART0.h"

#include <util/setbaud.h>


void
USART0_Init() {
    if (1) {
        // 9600
        UBRR0H = 0;
        UBRR0L = 207;
        UCSR0A |= (1 << U2X0);
    }
    if (0) {
        // 115200
        UBRR0H = 0;
        UBRR0L = 16;
        UCSR0A |= (1 << U2X0);
    }

    if (0) {
        const uint16_t normal = (uint16_t)(F_CPU / (16 * BAUD));
        UBRR0H = normal >> 8;
        UBRR0L = normal;
    }

    if (0) {
        UBRR0H = UBRRH_VALUE;
        UBRR0L = UBRRL_VALUE;
#if USE_2X
        UCSR0A |= (1 << U2X0);
#else
        UCSR0A &= ~(1 << U2X0);
#endif
    }

    // disable Multi-processor Communication Mode
    UCSR0A &= ~(1 << MPCM0);
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

