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

#include <avr/pgmspace.h>
#include <util/delay.h>
#include <string.h>

#include "RF24.h"
#include "../Misc.h"
#include "../Debug.h"

#ifndef RF24_DEBUG
#   undef DEBUG
#   define DEBUG(...)
#   undef DEBUG_P
#   define DEBUG_P(...)
#   undef ASSERT_FILE
#   define ASSERT_FILE(...)
#endif

static RF24MessageReceivedCallback s_RF24MessageReceivedCallback NOINIT;

#define RF24_TX_QUEUE_SIZE 3

typedef struct {
    uint8_t Buffer[RF24_TX_QUEUE_SIZE*RF24_MAX_PAYLOAD_SIZE];
    uint8_t Sizes[RF24_TX_QUEUE_SIZE];
    uint8_t Size   : 4;
    uint8_t Offset : 4;
} TxQueueData;

static TxQueueData s_TxQueueData;


void
RF24_SPI_Run(uint8_t* buffer, uint8_t size) {
    SPI_Master_Start_Transmission();
    for (uint8_t i = 0; i < size; ++i) {
        SPI_Master_Exchange(buffer[i]);
    }
    SPI_Master_End_Transmission();
}

void
RF24_UpdateRegister(
    uint8_t reg,
    uint8_t value,
    uint8_t mask) {

    value &= mask;

    uint8_t buffer[] = { (uint8_t)(RF24_OP_READ_REG | reg), 0xff };
    RF24_SPI_Run(buffer, sizeof(buffer));

    if ((buffer[1] & mask) != value) { // value diffrent?
        buffer[0] = RF24_OP_WRITE_REG | reg;
        buffer[1] &= ~mask;
        buffer[1] |= value;
        RF24_SPI_Run(buffer, sizeof(buffer));
    }
}

void
RF24_SetRxBaseAddress(uint32_t addr) {
    const uint8_t byteAddr[] = { (uint8_t)(addr & 0xff), (uint8_t)((addr >> 8) & 0xff), (uint8_t)((addr >> 16) & 0xff), (uint8_t)((addr >> 24) & 0xff) };
    uint8_t buffer[] = { RF24_OP_READ_REG | RF24_REG_RX_ADDR_P0, 0xff, 0xff, 0xff, 0xff, 0xff };
    RF24_SPI_Run(buffer, sizeof(buffer));
    if (buffer[2] != byteAddr[0] ||
        buffer[3] != byteAddr[1] ||
        buffer[4] != byteAddr[2] ||
        buffer[5] != byteAddr[3]) {
        // pipe 0
        buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_ADDR_P0;
        buffer[2] = byteAddr[0];
        buffer[3] = byteAddr[1];
        buffer[4] = byteAddr[2];
        buffer[5] = byteAddr[3];
        RF24_SPI_Run(buffer, sizeof(buffer));
        // pipe 1
        buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_ADDR_P1;
        buffer[2] = byteAddr[0];
        buffer[3] = byteAddr[1];
        buffer[4] = byteAddr[2];
        buffer[5] = byteAddr[3];
        RF24_SPI_Run(buffer, sizeof(buffer));
    }
}


void
RF24_SetRxAddresses(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4, uint8_t p5) {
    uint8_t buffer[] = { RF24_OP_READ_REG | RF24_REG_RX_ADDR_P0, 0xff, 0xff, 0xff, 0xff, 0xff };
    RF24_SPI_Run(buffer, sizeof(buffer));

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_ADDR_P0;
    buffer[1] = p0;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_ADDR_P1;
    buffer[1] = p1;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_ADDR_P2;
    buffer[1] = p2;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_ADDR_P3;
    buffer[1] = p3;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_ADDR_P4;
    buffer[1] = p4;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_ADDR_P5;
    buffer[1] = p5;
    RF24_SPI_Run(buffer, 2);
}


void
RF24_SetRxPayloadSizes(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4, uint8_t p5) {
    uint8_t buffer[2];
    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_PW_P0;
    buffer[1] = p0;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_PW_P1;
    buffer[1] = p1;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_PW_P2;
    buffer[1] = p2;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_PW_P3;
    buffer[1] = p3;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_PW_P4;
    buffer[1] = p4;
    RF24_SPI_Run(buffer, 2);

    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_RX_PW_P5;
    buffer[1] = p5;
    RF24_SPI_Run(buffer, 2);
}


void
RF24_InterruptHandler() {
    uint8_t buffer[RF24_MAX_PAYLOAD_SIZE + 1];
    uint8_t status, fifo;
    uint8_t again = 1;

    while (again) {
//        RF24_Dump(s_FILE_Debug);
        again = 0;
        buffer[0] = RF24_OP_READ_REG | RF24_REG_FIFO_STATUS;
        RF24_SPI_Run(buffer, 2);
        status = buffer[0];
        fifo = buffer[1];

        if (!(fifo & RF24_FIFO_STATUS_RX_EMPTY)) {
            const uint8_t pipe = (status & RF24_STATUS_RX_P_NO) >> 1;
            if (pipe < 6) {
//                DEBUG_P("fetch size of message in pipe %u\n", pipe);
                // download the message
                buffer[0] = RF24_OP_READ_REG | (pipe + 0x11);
                RF24_SPI_Run(buffer, 2);

                const uint8_t size = buffer[1];
                if (size) { // looks like sometimes we get 0 bytes
    //                DEBUG_P("download %u\n", size);
                    buffer[0] = RF24_OP_READ_RX_PAYLOAD;
                    RF24_SPI_Run(buffer, size + 1);

                    // forward to callback
                    if (s_RF24MessageReceivedCallback) {
                        //DEBUG("call\n");
                        s_RF24MessageReceivedCallback(&buffer[1], size);
                    }
                }
            }

            again = 1;
        }
    }

//    DEBUG("clear int\n");
    // clear any interrupt flags
    buffer[0] = RF24_OP_WRITE_REG | RF24_REG_STATUS;
    buffer[1] = RF24_STATUS_RX_DR | RF24_STATUS_TX_DS | RF24_STATUS_MAX_RT;
    RF24_SPI_Run(buffer, 2);
}

void
RF24_SetMessageReceivedCallback(
    RF24MessageReceivedCallback callback) {
    s_RF24MessageReceivedCallback = callback;
}

void
RF24_UploadTx(const uint8_t* ptr, uint8_t size) {
    ASSERT_FILE(ptr, return, "rf24");
    ASSERT_FILE(size, return, "rf24");
    SPI_Master_Start_Transmission();
    uint8_t data = RF24_OP_WRITE_TX_PAYLOAD;
    SPI_Master_Exchange(data);
    for (uint8_t i = 0; i < size; ++i) {
        data = ptr[i];
        SPI_Master_Exchange(data);
    }
    SPI_Master_End_Transmission();
}

void
RF24_Send(const uint8_t* ptr, uint8_t size) {
    RF24_TxQueueSubmit(ptr, size);
    RF24_TxQueueProcess();
}

void
RF24_Dump(FILE *f) {
    ASSERT_FILE(f, return, "rf24");
    uint8_t buffer[6];

    const char* const PowerMap[] = {
        "-18dBm",
        "-12dBm",
        "-6dBm",
        "0dBm"
    };

    uint8_t config;
    uint8_t en_aa;
    uint8_t en_rxaddr;
    uint8_t rf_ch;
    uint8_t rf_setup;
    uint8_t setup_aw;
    uint8_t status;
    uint8_t fifo_status;
    uint8_t feature;

    buffer[0] = RF24_OP_READ_REG | RF24_REG_CONFIG;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    status = buffer[0];
    config = buffer[1];
    buffer[0] = RF24_OP_READ_REG | RF24_REG_EN_AA;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    en_aa = buffer[1];
    buffer[0] = RF24_OP_READ_REG | RF24_REG_EN_RXADDR;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    en_rxaddr = buffer[1];
    buffer[0] = RF24_OP_READ_REG | RF24_REG_RF_CH;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    rf_ch = buffer[1];
    buffer[0] = RF24_OP_READ_REG | RF24_REG_RF_SETUP;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    rf_setup = buffer[1];
    buffer[0] = RF24_OP_READ_REG | RF24_REG_SETUP_AW;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    setup_aw = buffer[1];
    buffer[0] = RF24_OP_READ_REG | RF24_REG_FIFO_STATUS;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    fifo_status = buffer[1];
    buffer[0] = RF24_OP_READ_REG | RF24_REG_FEATURE;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    feature = buffer[1];

    const char* fmt = PSTR(
"CONFIG\n"
"  MASK_RX_DR:   %d\n"
"  MASK_TX_DS:   %d\n"
"  MASK_MAX_RT:  %d\n"
"  EN_CRC:       %d\n"
"  CRCO:         %d\n"
"  PWR_UP:       %d\n"
"  PRIM_RX:      %d (%s)\n"
"EN_AA:          %02x\n"
"EN_RXADDR:      %02x\n"
"RF_CH:          %d\n"
"RF_SETUP\n"
"  CONT_WAVE:    %d\n"
"  DR_LOW:       %d\n"
"  PLL_LOCK:     %d\n"
"  DR_HIGH:      %d\n"
"  PWR:          %d (%s)\n"
"SETUP_AW:       %d (%d)\n"
"STATUS\n"
"  RX_DR:        %d\n"
"  TX_DS:        %d\n"
"  MAX_RT:       %d\n"
"  RX_P_NO:      %d\n"
"  TX_FULL:      %d\n"
"FIFO_STATUS\n"
"  TX_REUSE:     %d\n"
"  TX_FULL:      %d\n"
"  TX_EMPTY:     %d\n"
"  RX_FULL:      %d\n"
"  RX_EMPTY:     %d\n"
"FEATURE:        %02x\n"
);
    fprintf_P(f, fmt,
        (config & RF24_CONFIG_MASK_RX_DR) ? 1 : 0,
        (config & RF24_CONFIG_MASK_TX_DS) ? 1 : 0,
        (config & RF24_CONFIG_MASK_MAX_RT) ? 1 : 0,
        (config & RF24_CONFIG_EN_CRC) ? 1 : 0,
        (config & RF24_CONFIG_CRCO) ? 1 : 0,
        (config & RF24_CONFIG_PWR_UP) ? 1 : 0,
        (config & RF24_CONFIG_PRIM_RX) ? 1 : 0, (config & RF24_CONFIG_PRIM_RX) ? "rx" : "tx",
        en_aa,
        en_rxaddr,
        rf_ch,
        (rf_setup & RF24_RF_SETUP_CONT_WAVE) ? 1 : 0,
        (rf_setup & RF24_RF_SETUP_RF_DR_LOW) ? 1 : 0,
        (rf_setup & RF24_RF_SETUP_RF_PLL_LOCK) ? 1 : 0,
        (rf_setup & RF24_RF_SETUP_RF_RF_DR_HIGH) ? 1 : 0,
        (rf_setup & RF24_RF_SETUP_RF_RF_PWR) >> 1, PowerMap[(rf_setup & RF24_RF_SETUP_RF_RF_PWR) >> 1],
        setup_aw, setup_aw + 2,
        (status & RF24_STATUS_RX_DR) ? 1 : 0,
        (status & RF24_STATUS_TX_DS) ? 1 : 0,
        (status & RF24_STATUS_MAX_RT) ? 1 : 0,
        (status & RF24_STATUS_RX_P_NO) >> 1,
        (status & RF24_STATUS_TX_FULL) ? 1 : 0,
        (fifo_status & RF24_FIFO_STATUS_TX_REUSE) ? 1 : 0,
        (fifo_status & RF24_FIFO_STATUS_TX_FULL) ? 1 : 0,
        (fifo_status & RF24_FIFO_STATUS_TX_EMPTY) ? 1 : 0,
        (fifo_status & RF24_FIFO_STATUS_RX_FULL) >> 1,
        (fifo_status & RF24_FIFO_STATUS_RX_EMPTY) ? 1 : 0,
        feature);



    buffer[0] = RF24_OP_READ_REG | RF24_REG_SETUP_RETR;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    fprintf_P(f, PSTR("Retrans %02x\n"), buffer[1]);



    buffer[0] = RF24_OP_READ_REG | RF24_REG_EN_RXADDR;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    fprintf_P(f, PSTR("Rx enaddr %02x\n"), buffer[1]);

//    buffer[0] = RF24_OP_READ_REG | RF24_REG_FEATURE;
//    buffer[1] = 0xff;
//    RF24_SPI_Run(buffer, 2);
//    fprintf_P(f, PSTR("Feature %02x\n"), buffer[1]);

    buffer[0] = RF24_OP_READ_REG | RF24_REG_RX_ADDR_P0;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 6);
    fprintf_P(f, PSTR("Rx p0 %02x%02x%02x%02x%02x\n"), buffer[5], buffer[4], buffer[3], buffer[2], buffer[1]);

    buffer[0] = RF24_OP_READ_REG | RF24_REG_RX_ADDR_P1;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 6);
    fprintf_P(f, PSTR("Rx p1 %02x%02x%02x%02x%02x\n"), buffer[5], buffer[4], buffer[3], buffer[2], buffer[1]);

    buffer[0] = RF24_OP_READ_REG | RF24_REG_RX_PW_P0;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    fprintf_P(f, PSTR("Rx p0 size %d\n"), buffer[1]);

    buffer[0] = RF24_OP_READ_REG | RF24_REG_RX_PW_P1;
    buffer[1] = 0xff;
    RF24_SPI_Run(buffer, 2);
    fprintf_P(f, PSTR("Rx p1 size %d\n"), buffer[1]);
}

void
RF24_Poll() {
    uint8_t data = RF24_OP_NOP;
    RF24_SPI_Run(&data, 1);
    if (data & RF24_STATUS_RX_DR) {
        //DEBUG_P("poll\n");
        //USART0_SendFlush();
        RF24_InterruptHandler();
        //DEBUG_P("poll done\n");
    }
}

void
RF24_TxQueueClear() {
    s_TxQueueData.Size = 0;
}

int8_t
RF24_TxQueueSubmit(const uint8_t* ptr, uint8_t size) {
    ASSERT_FILE(ptr, return 0, "rf24");
    ASSERT_FILE(size, return 0, "rf24");
    ASSERT_FILE(size <= RF24_MAX_PAYLOAD_SIZE, return 0, "rf24");

    if (RF24_TX_QUEUE_SIZE == s_TxQueueData.Size) {
        return 0;
    }

    //DEBUG_P("Enqueue to %u %u\n", s_TxQueueData.Offset, size);

    memcpy(&s_TxQueueData.Buffer[s_TxQueueData.Offset * RF24_MAX_PAYLOAD_SIZE], ptr, size);
    s_TxQueueData.Sizes[s_TxQueueData.Offset] = size;
    ++s_TxQueueData.Offset;
    if (s_TxQueueData.Offset == RF24_TX_QUEUE_SIZE) {
        s_TxQueueData.Offset = 0;
    }

    ++s_TxQueueData.Size;

    return 1;
}

void
RF24_TxQueueProcess() {
    uint8_t buffer[2];
    while (s_TxQueueData.Size) {
//        if (s_TxQueueData.Size > 1) {
//            DEBUG_P("#txq %u\n", s_TxQueueData.Size);
//        }

        //DEBUG_P("tx\n");
        // set TX mode
        RF24_SetTxMode();

        //DEBUG_P("flush rx\n");
        // Clear anything in the rx pipe.
        // This call is required, else tx fails
        //RF24_FlushRx();
        //RF24_FlushTx();

        //DEBUG_P("clear int\n");
        // clear all interrupt flags
        buffer[1] = RF24_STATUS_RX_DR | RF24_STATUS_MAX_RT | RF24_STATUS_TX_DS;
        buffer[0] = RF24_OP_WRITE_REG | RF24_REG_STATUS;
        RF24_SPI_Run(buffer, 2);

        for (uint8_t i = 0; i < s_TxQueueData.Size && i < 3; ++i, --s_TxQueueData.Size) {
            int8_t index = (s_TxQueueData.Offset - s_TxQueueData.Size);
            if (index < 0) {
                index += RF24_TX_QUEUE_SIZE;
            }

            uint8_t size = s_TxQueueData.Sizes[index];
            uint8_t* ptr = &s_TxQueueData.Buffer[index * RF24_MAX_PAYLOAD_SIZE];

//            DEBUG_P("Upload to tx queue %d %u\n", index, size);
            ASSERT_FILE(size, break, "rf24");

            RF24_UploadTx(ptr, size);
        }

        do {
            // wait for packet delivery
            do {
                // Sometimes the send will lock up
                // with status reg reporting 0x00 and
                // the value of fifo status reg being
                // all over the place.
                // Clearing the rx queue seems to fix
                // this.
                buffer[0] = RF24_OP_FLUSH_RX;
                RF24_SPI_Run(buffer, 1);
                //_delay_us(8);
                DEBUG_P("s");

            } while (!(buffer[0] & (RF24_STATUS_MAX_RT | RF24_STATUS_TX_DS)));

            //DEBUG_P("clear int\n");
            // clear interrupts
            buffer[1] = buffer[0];
            buffer[0] = RF24_OP_WRITE_REG | RF24_REG_STATUS;
            RF24_SPI_Run(buffer, 2);

            // check if all messages have been sent
            buffer[0] = RF24_OP_READ_REG | RF24_REG_FIFO_STATUS;
            RF24_SPI_Run(buffer, 2);
            DEBUG_P("q");

        } while ((buffer[1] & RF24_FIFO_STATUS_TX_EMPTY) != RF24_FIFO_STATUS_TX_EMPTY);

        //DEBUG_P("rx\n");
        // back to RX mode
        // so that 4ms max tx mode is honored
        RF24_SetRxMode();
    }
}
