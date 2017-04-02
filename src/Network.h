/* The MIT License (MIT)
 *
 * Copyright (c) 2016, 2017 Jean Gressmann <jean@0x42.de>
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

#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NETWORK_BROADCAST_ADDRESS   0xff
#define NETWORK_RXTX_DURATION       (UINT32_C(1<<14)) // 16s
//#define NETWORK_RXTX_DURATION       (UINT32_C(1<<13)) // 8s

//#define NETWORK_PERIOD     (UINT32_C(8)*NETWORK_RXTX_DURATION) // 2mins
//#define NETWORK_PERIOD     (UINT32_C(18)*NETWORK_RXTX_DURATION) // 5mins
//#define NETWORK_PERIOD     (UINT32_C(37)*NETWORK_RXTX_DURATION) // 10mins
#define NETWORK_PERIOD     (UINT32_C(75)*NETWORK_RXTX_DURATION) // 20mins

#define NETWORK_PACKET_PAYLOAD_SIZE 31

typedef struct {
    uint8_t Type : 2;
    uint8_t TTL  : 6;
    uint8_t Payload[NETWORK_PACKET_PAYLOAD_SIZE];
} NetworkPacket;

typedef void (*Network_SendCallback)(NetworkPacket* packet);
void Network_SetSendCallback(Network_SendCallback callback);
void Network_Send(NetworkPacket* packet);
uint8_t Network_GetAddress();
void Network_SetAddress(uint8_t id);
uint8_t Network_GetTtl();
void Network_SetTtl(uint8_t ttl);



#ifdef __cplusplus
}
#endif


#endif /* NETWORK_H */
