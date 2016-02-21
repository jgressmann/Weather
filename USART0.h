#ifndef USART0_H
#define USART0_H

#include <avr/io.h>
#include <util/delay.h>

#define USART0_WaitForActiveSend() loop_until_bit_is_set(UCSR0A, UDRE0)
#define USART0_SendFlush() \
    do { \
        USART0_WaitForActiveSend(); \
        /* 200 us works for Arduino to ensure data hits receiver @ 115200 */ \
        /* 200 us works for Arduino to ensure data hits receiver @ 9600 */ \
        _delay_us(2400); \
    } while (0)

#define USART0_SendByte(byte) \
    do { \
        /* Wait for empty transmit buffer */ \
        loop_until_bit_is_set(UCSR0A, UDRE0); \
        /* Put data into buffer, sends the data */ \
        UDR0 = byte; \
    } while (0)

#define USART0_SendString(str) \
    do { \
        const char* x = str; \
        while (*x) { \
            unsigned char c = (*x++); \
            USART0_SendByte(c); \
        } \
    } while (0)




#define USART0_HasReceivedByte() (UCSR0A & (1 << RXC0))
#define USART0_FetchReceivedByte() UDR0
extern void USART0_Init();
#define USART0_Uninit() UCSR0B = 0

#endif // USART0_H
