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

#include <util/delay.h>
#include "../Misc.h"
#if ARDUINO
#   include <Arduino.h>
#endif

#include "DHT.h"
#include "../Debug.h"

#ifndef NDEBUG
uint8_t g_DHT_Bits[40] NOINIT;
#endif

#if ARDUINO
int8_t
DHT11_Arduino_Read(int pin, uint8_t* temperature, uint8_t* humidity) {
    uint8_t buffer[5];
    uint8_t byte = 0;
    uint8_t bitsLeft = 8;
    uint8_t index = 0;
    uint8_t detectedHigh = 0;
    uint8_t detectedLow = 0;
    uint8_t highCount = 0;

    // Initialize communication
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    // at least 18 ms
    _delay_ms(20);
    // pull high, wait for 20-40 ms
    digitalWrite(pin, HIGH);

    // DHT will pull down pin for 80us, then high for 80us
    pinMode(pin, INPUT);
    // wait for DHT11 response low
    detectedLow = 0;
    for (uint8_t i = 0; i < 40; ++i) {
        if (digitalRead(pin) == LOW) {
            detectedLow = 1;
            break;
        }
        _delay_us(1);
    }

    if (!detectedLow) {
        return -1;
    }

    // wait for DHT11 response high
    detectedHigh = 0;
    for (uint8_t i = 0; i < 80; ++i) {
        _delay_us(1);
        if (digitalRead(pin) == HIGH) {
            detectedHigh = 1;
            break;
        }
    }

    if (!detectedHigh) {
        return -2;
    }

    // wait for start of transmission
    // make sure to start on low
    detectedLow = 0;
    for (uint8_t i = 0; i < 80; ++i) {
        if (digitalRead(pin) == LOW) {
            detectedLow = 1;
            break;
        }
        _delay_us(1);
    }

    if (!detectedLow) {
        return -3;
    }

    // [Data format: 8bit integral RH data + 8bit decimal RH data + 8bit integral T data + 8bit decimal T
    // data + 8bit check sum. If the data transmission is right, the check-sum should be the last 8bit of
    // "8bit integral RH data + 8bit decimal RH data + 8bit integral T data + 8bit decimal T data.
    for (uint8_t i = 0; i < 40; ++i) {
        // pin is LOW on enter
        for (uint8_t j = 0; j < 50; ++j) {
            if (digitalRead(pin) == HIGH) {
                detectedHigh = 1;
                break;
            }
            _delay_us(1);
        }

        if (!detectedHigh) {
            return -4;
        }


        // measure how long the high is, 26-28us mean 0, upward mean 1
        // total time for high is 70us
        highCount = 0;
        detectedLow = 0;
        for (uint8_t j = 0; j < 70; ++j) {
            if (digitalRead(pin) == LOW) {
                detectedLow = 1;
                break;
            }
            ++highCount;
            _delay_us(1);
        }

        if (!detectedLow) {
            return -5;
        }

        --bitsLeft;
        byte <<= 1;
        // well looks like with this code the
        // value for 0 is 3/4 hightCounts
        // and the value for 1 is 11/12
        if (highCount >= 7) {
            byte |= 1;
        } else {

        }
#ifndef NDEBUG
        g_DHT11_Bits[i] = highCount;
#endif
        if (!bitsLeft) {
            bitsLeft = 8;
            buffer[index++] = byte;
            byte = 0;
        }
    }

    // verify checksum
    const uint8_t sum = buffer[0] + buffer[1] + buffer[2] + buffer[3];
    if (sum  != buffer[4]) {
        return -100;
    }

    // according to spec resolution is 1 deg C / 1 %rH
    // which means we don't need fractional parts
    *humidity = buffer[0];
    *temperature = buffer[2];

    return 0;
}
#endif



static
int8_t
DHT_Read(
    DHT_Context* ctx,
    uint8_t* temperature,
    uint8_t* humidity,
    int8_t variant) {

    uint8_t buffer[5];
    uint8_t byte = 0;
    uint8_t bitsLeft = 8;
    uint8_t index = 0;
    uint8_t detectedHigh = 0;
    uint8_t detectedLow = 0;
    uint8_t highCount = 0;

    int8_t error = 0;
    volatile uint8_t* port = ctx->port;
    volatile uint8_t* pin = ctx->pin;
    volatile uint8_t* ddr = ctx->ddr;
    const uint8_t mask = ctx->mask;

    // NOTE: this code assumes PrepareRead has been called


    *port &= ~mask;

    switch (variant) {
    case 11:
        // pull low for at least 18 ms
        _delay_ms(20);
        break;
    case 22:
        _delay_ms(3);
        break;
    default:
        error = -16;
        goto Exit;
    }

    // DHT will pull down pin for 80us, then high for 80us
    *port |= mask;
    *ddr &= ~mask;
    *port &= ~mask;

    // wait for DHT11 response low
    detectedLow = 0;
    for (uint8_t i = 0; i < 40; ++i) {
        if (!((*pin) & mask)) {
            detectedLow = 1;
            break;
        }
        _delay_us(1);
    }

    if (!detectedLow) {
        error = -1;
        goto Exit;
    }

    // wait for DHT11 response high
    detectedHigh = 0;
    for (uint8_t i = 0; i < 80; ++i) {
        _delay_us(1);
        if (((*pin) & mask)) {
            detectedHigh = 1;
            break;
        }
    }

    if (!detectedHigh) {
        error = -2;
        goto Exit;
    }

    // wait for start of transmission
    // make sure to start on low
    detectedLow = 0;
    for (uint8_t i = 0; i < 80; ++i) {
        if (!((*pin) & mask)) {
            detectedLow = 1;
            break;
        }
        _delay_us(1);
    }

    if (!detectedLow) {
        error = -3;
        goto Exit;
    }

    // [Data format: 8bit integral RH data + 8bit decimal RH data + 8bit integral T data + 8bit decimal T
    // data + 8bit check sum. If the data transmission is right, the check-sum should be the last 8bit of
    // "8bit integral RH data + 8bit decimal RH data + 8bit integral T data + 8bit decimal T data.
    for (uint8_t i = 0; i < 40; ++i) {
        // pin is LOW on enter
        for (uint8_t j = 0; j < 50; ++j) {
            if (((*pin) & mask)) {
                detectedHigh = 1;
                break;
            }
            _delay_us(1);
        }

        if (!detectedHigh) {
            error = -4;
            goto Exit;
        }

        // measure how long the high is, 26-28us mean 0, upward mean 1
        // total time for high is 70us
        highCount = 0;
        detectedLow = 0;
        for (uint8_t j = 0; j < 70; ++j) {
            if (!((*pin) & mask)) {
                detectedLow = 1;
                break;
            }

            ++highCount;
            _delay_us(1);
        }

        if (!detectedLow) {
            error = -5;
            goto Exit;
        }

#if F_CPU >= 160000000L
#   define HIGHCOUNT 32
#else
#    define HIGHCOUNT 24
#endif

        --bitsLeft;
        byte <<= 1;
        /* Printing ' ' + highCount yields
         * @  8Mhz: +,C,C,+D+,+,,+,,,+,C,+D++,+,,+,,,%DCC,CB
         *  ==> 0 is 16/17 hightCounts and the value for 1 is 47/48
         *
         *          ++++++B+,,CBB+B+,,,,,,,,BB,,CBBA,,,,B,BA
         *
         *
         * @ 16Mhz: 00O0O//P//////01///L00O/00/////000POO0OM
         *  ==> 0 is 11/12 hightCounts and the value for 1 is 35/36
         *
         */

        if (highCount >= HIGHCOUNT) {
            byte |= 1;
        } else {

        }
#ifndef NDEBUG
        g_DHT_Bits[i] = highCount;
#endif
        if (!bitsLeft) {
            bitsLeft = 8;
            buffer[index++] = byte;
            byte = 0;
        }
    }

    // verify checksum
    const uint8_t sum = buffer[0] + buffer[1] + buffer[2] + buffer[3];
    if (sum  != buffer[4]) {
        error = -17;
        goto Exit;
    }

    // according to spec resolution is 1 deg C / 1 %rH
    // which means we don't need fractional parts
    switch (variant) {
    case 11:
        *humidity = buffer[0];
        *temperature = buffer[2];
        break;
    case 22: {
            int16_t h = buffer[0];
            h <<= 8;
            h |= buffer[1];
            h /= 10;
            *humidity = h;
            int16_t t = buffer[2];
            t <<= 8;
            t |= buffer[3];
            t /= 10;
            *temperature = t;
        } break;
    }

Exit:
    return error;
}

int8_t
DHT11_Read(
    DHT_Context* ctx,
    uint8_t* temperature,
    uint8_t* humidity) {
    return DHT_Read(ctx, temperature, humidity, 11);
}

int8_t
DHT22_Read(
    DHT_Context* ctx,
    uint8_t* temperature,
    uint8_t* humidity) {
    return DHT_Read(ctx, temperature, humidity, 22);
}

void
DHT_PrepareRead(DHT_Context* ctx) {
    // Setup the port for high out
    *ctx->port &= ~ctx->mask; // tri-state transition
    *ctx->ddr |= ctx->mask;
    // pull high, wait a bit so that DHT11 can detect high and sync to that
    *ctx->port |= ctx->mask;
}
