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


#ifndef DHT_H
#define DHT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if ARDUINO
int8_t
DHT11_Arduino_Read(int pin, uint8_t* temperature, uint8_t* humidity);
#endif

#define DHT11_PREPARE_TIME_MS 200 /* 180ms is too short */
#define DHT22_PREPARE_TIME_MS 750 /* 500 is too short for DHT22 */

typedef struct {
    volatile uint8_t* ddr;
    volatile uint8_t* port;
    volatile uint8_t* pin;
    uint8_t mask;
} DHT_Context;

void DHT_PrepareRead(DHT_Context* ctx);
#define DHT11_IsReadPrepared(millisSinceStart) ((millisSinceStart) >= DHT11_PREPARE_TIME_MS)
#define DHT22_IsReadPrepared(millisSinceStart) ((millisSinceStart) >= DHT22_PREPARE_TIME_MS)
int8_t DHT11_Read(
    DHT_Context* ctx,
    uint8_t* temperature,
    uint8_t* humidity);

int8_t DHT22_Read(
    DHT_Context* ctx,
    uint8_t* temperature,
    uint8_t* humidity);

#ifndef NDEBUG
extern uint8_t g_DHT_Bits[40];
#endif

#ifdef __cplusplus
}
#endif

#endif /* DHT_H */

