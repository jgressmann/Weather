#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdio.h>

#if ARDUINO
#   include <Arduino.h>
#endif
#include "../../Misc.h"
#include "../Debug.h"
#include "../USART0.h"
#include "../SPI.h"
#include "../RF24.h"


#define RF24_PIPE_BASE_ADDRESS 0xdeadbeef
static uint8_t s_RF24_Channel = 76;
static uint8_t s_RF24_Power = RF24_TX_PWR_MAX;
static uint8_t s_RF24_DataRate = RF24_DR_2MBPS;

static FILE* s_FILE_USART0;
static
int
USART0_PutChar(char c, FILE *stream) {

    (void)stream;
    USART0_SendByte(c);
    return 0;
}

static
int
USART0_GetChar(FILE *stream) {

    (void)stream;
    if (USART0_HasReceivedByte()) {
        return USART0_FetchReceivedByte();
    }

    return EOF;
}



static
void
RF24_Init() {
    // We don't know in what state we get the device so
    // reset everything


    // The CE line needs to be high to actually perform RX/TX
#ifdef HWRV11
    DDRD |= _BV(PD7); // pin 7 -> output
    PORTD &= ~_BV(PD7); // low, this is the CE line
#else
    DDRB |= _BV(PB1); // pin 9 -> output
    PORTB &= ~_BV(PB1); // low, this is the CE line
#endif

#if F_CPU >= 16000000L
    SPI_Master_Init(SPI_MSB_FIRST, SPI_CLOCK_DIV_4, SPI_CLOCK_SPEED_1X, SPI_MODE_0);
#else
    SPI_Master_Init(SPI_MSB_FIRST, SPI_CLOCK_DIV_4, SPI_CLOCK_SPEED_2X, SPI_MODE_0);
#endif

#ifndef HWRV11
    // Arduino pin 8 (PB0 on ATmega328P) is connected to the interrupt line
    DDRB &= ~_BV(PB0); // pin 8
    // activate pull up resistor
    PORTB |= _BV(PB0);

    // enable PCINT0
    PCICR |= _BV(PCIE0);
    PCMSK0 |= _BV(PCINT0);
    PCIFR = 0; // clear any flags
#endif


#ifdef HWRV11
    RF24_SetInterruptMask(RF24_IRQ_MASK_MAX_RT | RF24_IRQ_MASK_TX_DS | RF24_IRQ_MASK_RX_DR);
#else
    RF24_SetInterruptMask(RF24_IRQ_MASK_MAX_RT | RF24_IRQ_MASK_TX_DS);
    //RF24_SetInterruptMask(RF24_IRQ_MASK_MAX_RT | RF24_IRQ_MASK_TX_DS | RF24_IRQ_MASK_RX_DR);
#endif
    RF24_SetCrc(RF24_CRC_16);
    RF24_SetChannel(s_RF24_Channel);
    // Looks like using byte zero won't work
    RF24_SetAddressWidth(RF24_ADDR_WITDH_5);
    RF24_SetTxAddress(RF24_PIPE_BASE_ADDRESS, 0x01);
    RF24_SetRxBaseAddress(RF24_PIPE_BASE_ADDRESS);
    RF24_SetRxAddresses(0x01, 0, 0, 0, 0, 0);
    RF24_SetRxPipeEnabled(0x01);
    RF24_SetRxPayloadSizes(RF24_MAX_PAYLOAD_SIZE, 0, 0, 0, 0, 0);
    RF24_SetDataRate(s_RF24_DataRate);
    RF24_SetTxPower(s_RF24_Power);

    // These too lines effectively disable Enhanced Shockburst
    RF24_SetPipeAutoAcknowledge(0);
    RF24_SetTxRetries(15, 0);

    // clear any pending interrupts
    RF24_UpdateRegister(RF24_REG_STATUS, RF24_STATUS_MAX_RT | RF24_STATUS_TX_DS | RF24_STATUS_RX_DR, RF24_STATUS_MAX_RT | RF24_STATUS_TX_DS | RF24_STATUS_RX_DR);

    RF24_PowerUp();
    RF24_SetRxMode();
    RF24_FlushTx();
    RF24_FlushRx();


    // finally set CE
#ifdef HWRV11
    PORTD |= _BV(PD7);
#else
    PORTB |= _BV(PB1);
#endif

#ifndef NDEBUG
    RF24_Dump(s_FILE_Debug);
#endif
}




ArduinoDeclare()

#define ever (;;)


#ifndef NDEBUG
extern "C" {
FILE* s_FILE_Debug NOINIT;
}
#endif


int
main() {
#if ARDUINO
    ArduinoInit();
#endif

    USART0_Init();
    s_FILE_USART0 = fdevopen(USART0_PutChar, USART0_GetChar);
#ifndef NDEBUG
    stderr = s_FILE_Debug = s_FILE_USART0;
#endif
    fprintf_P(s_FILE_USART0, PSTR("rf24\n"));


    RF24_Init();

    uint32_t counter = 0;
    char buffer[RF24_MAX_PAYLOAD_SIZE];

    for ever {

        snprintf_P(buffer, sizeof(buffer), PSTR("ping %lu\n"), counter++);
        fprintf(s_FILE_USART0, buffer);
        RF24_SetTxMode();
        RF24_Send((uint8_t*)buffer, sizeof(buffer));
        RF24_SetRxMode();
        _delay_ms(1000);
    }

    return 0;
}

