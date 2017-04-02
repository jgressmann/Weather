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

<<<<<<< HEAD
#ifndef TIME_H
#define TIME_H
=======
#ifndef TIME585858_H
#define TIME585858_H
>>>>>>> r2

#include <stdint.h>

#include "Network.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIME_PACKET_TYPE            0x00
#define TIME_INT_START              0
#define TIME_INT_SEND_RECEIVE_START 1
#define TIME_INT_SEND_RECEIVE_STOP  2
#define TIME_INT_STOP               3

typedef void (*Time_SyncWindowCallback)(int8_t what);

void Time_Init();
void Time_Uninit();
uint32_t Time_Now();

void Time_SetSyncWindowCallback(Time_SyncWindowCallback callback);
void Time_Update(uint16_t millisecondsElapsed);
void Time_Process(NetworkPacket* packet);
int8_t Time_IsSynced();

void Time_Sync(uint8_t enable);
void Time_Broadcast(uint8_t enable);
uint32_t Time_TimeToNextInterval();
uint32_t Time_MillisecondsTillNextWindow();
void Time_NotifyStartListening(uint8_t scan);
void Time_NotifyStopListening();
void Time_SetStratum(int16_t stratum);
<<<<<<< HEAD
=======
void Time_BroadcastTime();
>>>>>>> r2


#ifdef __cplusplus
}
#endif

<<<<<<< HEAD
#endif /* TIME_H */
=======
#endif /* TIME585858_H */
>>>>>>> r2

