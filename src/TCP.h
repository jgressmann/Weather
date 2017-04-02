/* The MIT License (MIT)
 *
<<<<<<< HEAD
 * Copyright (c) 2016 Jean Gressmann <jean@0x42.de>
=======
 * Copyright (c) 2016, 2017 Jean Gressmann <jean@0x42.de>
>>>>>>> r2
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

#ifndef TCP_H
#define TCP_H

#include <stdint.h>
#include "Network.h"


#ifdef __cplusplus
extern "C" {
#endif


#define TCP_PACKET_TYPE          0x02
<<<<<<< HEAD
#define TCP_PAYLOAD_SIZE        (NETWORK_PACKET_PAYLOAD_SIZE-5)
=======
#define TCP_PAYLOAD_SIZE        (NETWORK_PACKET_PAYLOAD_SIZE-6)
>>>>>>> r2

typedef void (*TCP_DataReceivedCallback)(uint8_t sender, const uint8_t* payload, uint8_t size);

void TCP_Init();
void TCP_Uninit();
void TCP_Update();
void TCP_Process(NetworkPacket* packet);
void TCP_Send(uint8_t destination, const uint8_t* ptr, uint8_t bytes);
void TCP_Purge();
void TCP_SetDataReceivedCallback(TCP_DataReceivedCallback callback);
void TCP_Decode(NetworkPacket* packet, uint8_t* sender, uint8_t* destination, uint8_t** ptr, uint8_t* bytes);

#ifdef __cplusplus
}
#endif

#endif /* TCP_H */
