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

#ifndef RF24_H
#define RF24_H

#include <inttypes.h>
#include <stdio.h>
#include "SPI.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Nordic nRF24L01+ (abbreviated as RF24) code
 *
 * The device is assumed to be connected to the
 * SPI interface of the ATMega processor. That
 * is pins MOSI, MISO, SCK, SS.
 *
 * This code does NOT handle the CE line
 * to actually enter RX/TX mode. You need
 * to do this yourself.
 */

/* RF24 constants */
#define RF24_MAX_PAYLOAD_SIZE   32
#define RF24_READ_PIPES         6

/* RF24 registers */
#define RF24_REG_CONFIG             0x00
#define RF24_REG_EN_AA              0x01
#define RF24_REG_EN_RXADDR          0x02
#define RF24_REG_SETUP_AW           0x03
#define RF24_REG_SETUP_RETR         0x04
#define RF24_REG_RF_CH              0x05
#define RF24_REG_RF_SETUP           0x06
#define RF24_REG_STATUS             0x07
#define RF24_REG_RX_ADDR_P0         0x0A
#define RF24_REG_RX_ADDR_P1         0x0B
#define RF24_REG_RX_ADDR_P2         0x0C
#define RF24_REG_RX_ADDR_P3         0x0D
#define RF24_REG_RX_ADDR_P4         0x0E
#define RF24_REG_RX_ADDR_P5         0x0F
#define RF24_REG_RX_PW_P0           0x11
#define RF24_REG_RX_PW_P1           0x12
#define RF24_REG_RX_PW_P2           0x13
#define RF24_REG_RX_PW_P3           0x14
#define RF24_REG_RX_PW_P4           0x15
#define RF24_REG_RX_PW_P5           0x16
#define RF24_REG_TX_ADDR            0x10
#define RF24_REG_FIFO_STATUS        0x17
#define RF24_REG_FEATURE            0x1D

/* Various flags in registers */
#define RF24_CONFIG_PRIM_RX         0x01
#define RF24_CONFIG_PWR_UP          0x02
#define RF24_CONFIG_CRCO            0x04
#define RF24_CONFIG_EN_CRC          0x08
#define RF24_CONFIG_MASK_MAX_RT     0x10
#define RF24_CONFIG_MASK_TX_DS      0x20
#define RF24_CONFIG_MASK_RX_DR      0x40

#define RF24_RF_SETUP_RF_RF_PWR     0x06
#define RF24_RF_SETUP_RF_RF_DR_HIGH 0x08
#define RF24_RF_SETUP_RF_PLL_LOCK   0x10
#define RF24_RF_SETUP_RF_DR_LOW     0x20
#define RF24_RF_SETUP_CONT_WAVE     0x80


#define RF24_FIFO_STATUS_TX_REUSE   0x40
#define RF24_FIFO_STATUS_TX_FULL    0x20
#define RF24_FIFO_STATUS_TX_EMPTY   0x10
#define RF24_FIFO_STATUS_RX_FULL    0x02
#define RF24_FIFO_STATUS_RX_EMPTY   0x01


#define RF24_STATUS_RX_DR           0x40
#define RF24_STATUS_TX_DS           0x20
#define RF24_STATUS_MAX_RT          0x10
#define RF24_STATUS_RX_P_NO         0x0e
#define RF24_STATUS_TX_FULL         0x01

/* RF24 ops */
#define RF24_OP_WRITE_REG          0x20
#define RF24_OP_READ_REG           0x00
#define RF24_OP_READ_RX_PAYLOAD    0b01100001
#define RF24_OP_READ_RX_PAYLOAD    0b01100001
#define RF24_OP_WRITE_TX_PAYLOAD   0b10100000
#define RF24_OP_FLUSH_RX           0b11100010
#define RF24_OP_FLUSH_TX           0b11100001
#define RF24_OP_NOP                0xff

/* CRC values */
#define RF24_CRC_NONE              0
#define RF24_CRC_8                 8
#define RF24_CRC_16                12

/* Radio power settings */
#define RF24_TX_PWR_M18DB      0
#define RF24_TX_PWR_M12DB      2
#define RF24_TX_PWR_M6DB       4
#define RF24_TX_PWR_MAX        6

/* Data rate */
#define RF24_DR_250KBPS     0x20
#define RF24_DR_1MBPS       0x00
#define RF24_DR_2MBPS       0x08

/* Interrupts */
#define RF24_IRQ_MASK_RX_DR     64
#define RF24_IRQ_MASK_TX_DS     32
#define RF24_IRQ_MASK_MAX_RT    16

/* Address width */
#define RF24_ADDR_WITDH_3   1
#define RF24_ADDR_WITDH_4   2
#define RF24_ADDR_WITDH_5   3

/* Features */
#define RF24_FEAT_EN_DPL        0x4
#define RF24_FEAT_EN_ACK_PAY    0x2
#define RF24_FEAT_EN_DYN_ACK    0x1



/* Run RF24 command over SPI */
void RF24_SPI_Run(uint8_t* buffer, uint8_t size);

/* Update single byte register */
void RF24_UpdateRegister(uint8_t reg, uint8_t value, uint8_t mask);

static
inline
void
RF24_PowerUp() {
    RF24_UpdateRegister(RF24_REG_CONFIG, RF24_CONFIG_PWR_UP, RF24_CONFIG_PWR_UP);
}

static
inline
void
RF24_PowerDown() {
    RF24_UpdateRegister(RF24_REG_CONFIG, 0, RF24_CONFIG_PWR_UP);
}

static
inline
void
RF24_SetCrc(uint8_t crc) {
    RF24_UpdateRegister(RF24_REG_CONFIG, crc, RF24_CRC_16);
}

static
inline
void
RF24_SetPipeAutoAcknowledge(uint8_t value) {
    RF24_UpdateRegister(RF24_REG_EN_AA, value, 63);
}

static
inline
void
RF24_SetRxPipeEnabled(uint8_t value) {
    RF24_UpdateRegister(RF24_REG_EN_RXADDR, value, 63);
}

static
inline
void
RF24_SetChannel(uint8_t value) {
    RF24_UpdateRegister(RF24_REG_RF_CH, value, 127);
}

static
inline
void
RF24_SetAddressWidth(uint8_t value) {
    RF24_UpdateRegister(RF24_REG_SETUP_AW, value, 3);
}

static
inline
void
RF24_SetTxPower(uint8_t value) {
    RF24_UpdateRegister(RF24_REG_RF_SETUP, value, 6);
}

static
inline
void
RF24_SetDataRate(uint8_t value) {
    RF24_UpdateRegister(RF24_REG_RF_SETUP, value, 0x28);
}

static
inline
void
RF24_SetInterruptMask(uint8_t value) {
    RF24_UpdateRegister(RF24_REG_CONFIG, value, 0x70);
}

/* Sets the number of retries and the delay between failed transmissions
 *
 * retransmitDelay: value in range 0..15 (250us..4000us)
 * retransmitCount: value in range 0..15 (0 = no retransmission)
 */
static
inline
void
RF24_SetTxRetries(uint8_t retransmitDelay, uint8_t retransmitCount) {
    uint8_t buffer[] = { RF24_OP_WRITE_REG | RF24_REG_SETUP_RETR, (uint8_t)(((retransmitDelay & 15) << 4) | (retransmitCount & 15)) };
    RF24_SPI_Run(buffer, sizeof(buffer));
}

static
inline
void
RF24_SetTxAddress(uint32_t base, uint8_t lsb) {
    uint8_t buffer[] = { RF24_OP_WRITE_REG | RF24_REG_TX_ADDR, lsb, (uint8_t)(base & 0xff), (uint8_t)((base >> 8) & 0xff), (uint8_t)((base >> 16) & 0xff), (uint8_t)((base >> 24) & 0xff) };
    RF24_SPI_Run(buffer, sizeof(buffer));
}

static
inline
void
RF24_SetRxMode() {
    RF24_UpdateRegister(RF24_REG_CONFIG, 1, 1);
}

static
inline
void
RF24_SetTxMode() {
    RF24_UpdateRegister(RF24_REG_CONFIG, 0, 1);
}

static
inline
void
RF24_FlushTx() {
    uint8_t buffer[] = { RF24_OP_FLUSH_TX };
    RF24_SPI_Run(buffer, sizeof(buffer));
}

static
inline
void
RF24_FlushRx() {
    uint8_t buffer[] = { RF24_OP_FLUSH_RX };
    RF24_SPI_Run(buffer, sizeof(buffer));
}

/* Sets the base address receive (most significant 4 bytes) for all pipes */
void RF24_SetRxBaseAddress(uint32_t addr);

/* Set the least significant byte of all receive pipes */
void RF24_SetRxAddresses(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4, uint8_t p5);

/* Sets the expected payload sizes for all pipes */
void RF24_SetRxPayloadSizes(uint8_t p0, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4, uint8_t p5);

/* Uploads the data to the device for sending
 *
 * You are responsible for
 *
 * - disabling interrupts before calling this function
 * - ensuring that the size doesn't exceed 32 bytes
 */
void RF24_UploadTx(const uint8_t* ptr, uint8_t size);

typedef void (*RF24MessageReceivedCallback)(uint8_t* ptr, uint8_t size);
void RF24_SetMessageReceivedCallback(RF24MessageReceivedCallback callback);


/* Interrupt handler for RF24 */
void RF24_InterruptHandler();


/* Clears queue of messages to be sent */
void RF24_TxQueueClear();

/* Adds message to transmit queue
 *
 * The message will be send next time
 *
 * RF24_Send is
 */
int8_t RF24_TxQueueSubmit(const uint8_t* ptr, uint8_t size);

/* Processes the send queue and transmits messages */
void RF24_TxQueueProcess();

/* Transmits a single message
 *
 * You are responsible for
 *
 * - ensuring CE line is high
 * - disabling interrupts before calling this function
 * - ensuring that the size doesn't exceed 32 bytes
 *
 * For the duration of the transmit the device is switched to TX mode.
 * Once the data has been sent or the sent fails after retries the
 * device is switched back to RX mode.
 */
void RF24_Send(const uint8_t* ptr, uint8_t size);

/* Dumps RF24 configuratio to file */
void RF24_Dump(FILE* f);

/* Polls for received data.
 *
 * Invokes RF24_InterruptHandler if there is data.
 */
void RF24_Poll();

#ifdef __cplusplus
}
#endif

#endif /* RF24_H */
