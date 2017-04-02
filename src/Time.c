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

#include "Time.h"
#include "Misc.h"
#include "Debug.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#if AVR
#   include <avr/sfr_defs.h>
#else
#   define _BV(x) (1<<(x))
#endif

#ifndef TIME_DEBUG
#   undef DEBUG
#   define DEBUG(...)
#   undef DEBUG_P
#   define DEBUG_P(...)
#endif


#define NOT_SYNCED_MARKER 0xff

#define SOURCE_CHAIN_LENGTH (NETWORK_PACKET_PAYLOAD_SIZE - 13)

typedef struct {
    uint32_t TimeToInterval;
    uint32_t LocalTime;
    uint16_t SequenceNumber;
    uint8_t Sender;
    uint8_t Originator;
    uint8_t Stratum;
    uint8_t Sources[SOURCE_CHAIN_LENGTH];
} Time_Payload;




#define TIME_PURGE_TIMEOUT      (UINT32_C(3600)*UINT32_C(1000)) /* 60 minutes in ms */
#define TIME_BROADCAST_INTERVAL (NETWORK_RXTX_DURATION/128)



#define TIME_SYNC               0
#define TIME_BROADCAST          1
#define TIME_AUTO_STRATUM       2
#define TIME_LISTENING          3
#define TIME_SCAN               4
#define TIME_M1                 5
#define TIME_M2                 6
#define TIME_INTERVAL           7


typedef struct _Peer {
    uint32_t PeerTimesToInterval[2];
    uint32_t PeerReplyTimes[2];
    //uint32_t PeerOneWayDelay[2];
    uint32_t PeerTwoWayDelaySum;
    uint32_t TimeToIntervalOfRequest;
    uint32_t TimeOfRequest;
    uint32_t LastAwareTime;
    //int16_t Offsets[4];
    uint16_t LastSequenceNumber;
    struct _Peer* Next;
    uint8_t Address;
    uint8_t Stratum;
    uint8_t PeerReplyCount;
    uint8_t PeerReplyIndex  : 2;
    //uint8_t PeerReplyCount  : 2;
//    uint8_t OffsetIndex     : 2;
//    uint8_t OffsetCount     : 3;
    uint8_t RequestReceived : 1;
    uint8_t Sources[SOURCE_CHAIN_LENGTH];
} Peer;


static uint32_t s_Mono NOINIT;
static uint32_t s_TimeToNextInterval NOINIT;
static uint32_t s_LastBroadcastTime NOINIT;
static uint32_t s_StartOfInterval NOINIT;
static uint16_t s_SequenceNumber NOINIT;
#ifdef TIME_DEBUG
static uint32_t s_Counts[3] NOINIT;
static uint32_t s_Errors NOINIT;
#endif
static uint8_t s_Flags NOINIT;
static uint8_t s_Stratum NOINIT;
<<<<<<< HEAD
static uint8_t s_Sources[SOURCE_CHAIN_LENGTH] NOINIT;
=======
static uint8_t s_Sources[SOURCE_CHAIN_LENGTH] NOINIT; // to prevent circular time dependencies
>>>>>>> r2
static Peer* s_Peers NOINIT;
static Time_SyncWindowCallback s_Callback NOINIT;

static
inline
void
ClearSourceChainPtr(uint8_t* chain) {
    chain[0] = NOT_SYNCED_MARKER;
}

static
inline
void
ClearSourceChain() {
    ClearSourceChainPtr(s_Sources);
}

static
void
PrunePeers() {
    Peer* newHead = NULL;
    while (s_Peers) {
        Peer* p = s_Peers;
        s_Peers = s_Peers->Next;
        if (IsInWindow32(s_Mono, TIME_PURGE_TIMEOUT, p->LastAwareTime)) {
            p->Next = newHead;
            newHead = p;
        } else {
            DEBUG_P("Time: prune peer %02x\n", p->Address);
            free(p);
        }
    }
    s_Peers = newHead;
}

static
Peer*
FindPeer(uint8_t address) {
    for (Peer* p = s_Peers; p; p = p->Next) {
        if (p->Address == address) {
            return p;
        }
    }
    return NULL;
}

static
Peer*
GetOrCreatePeer(uint8_t address) {
    Peer* p = FindPeer(address);
    if (!p) {
        p = malloc(sizeof(*p));
        if (p) {
            DEBUG_P("Time: create peer %02x\n", address);
            memset(p, 0, sizeof(*p));
            p->Address = address;
            p->LastSequenceNumber = s_SequenceNumber - 1;
            p->Next = s_Peers;
            s_Peers = p;
        }
    }
    return p;
}

void
Time_Init() {
    s_Mono = 0;
    s_TimeToNextInterval = 0;
    s_StartOfInterval = 0;
    s_Flags = _BV(TIME_SYNC) | _BV(TIME_BROADCAST) | _BV(TIME_AUTO_STRATUM);
    s_Peers = NULL;
    s_Stratum = NOT_SYNCED_MARKER;
    s_Callback = NULL;
    s_LastBroadcastTime = 0;
    ClearSourceChain();
#ifdef TIME_DEBUG
    s_Errors  = 0;
    memset(s_Counts, 0, sizeof(s_Counts));
#endif
    DEBUG_P("Time: init\n");
}

void
Time_Uninit() {
    DEBUG_P("Time: uninit\n");
    while (s_Peers) {
        Peer* p = s_Peers;
        s_Peers = s_Peers->Next;
        free(p);
    }
}

uint32_t
Time_Now() {
    return s_Mono;
}

int8_t
Time_IsSynced() {
    if (s_Stratum != NOT_SYNCED_MARKER) {
        DEBUG_P("Time: :)\n");
        return 1;
    }

    DEBUG_P("Time: :(\n");
    return 0;
}


static
void
BroadcastMessage() {
    ASSERT_FILE(s_Stratum != NOT_SYNCED_MARKER, return, "time");

    NetworkPacket packet;
    packet.Type = TIME_PACKET_TYPE;
    packet.TTL = Network_GetTtl();
    Time_Payload* t = (Time_Payload*)packet.Payload;
    t->Sender = Network_GetAddress();
    t->Originator = t->Sender;
    t->SequenceNumber = s_SequenceNumber;
    t->Stratum = s_Stratum;
    t->TimeToInterval = s_TimeToNextInterval;
    t->LocalTime = s_Mono;
    memcpy(t->Sources, s_Sources, sizeof(s_Sources));

    Network_Send(&packet);

//    DEBUG_P("Time: bc\n");
}

static
inline
void
TerminateInterval() {
    if (s_Callback) {
        s_Callback(TIME_INT_STOP);
    }
    s_Flags &= ~(_BV(TIME_INTERVAL) | _BV(TIME_M1) | _BV(TIME_M2));
}

void
Time_Update(uint16_t millisecondsElapsed) {
    s_Mono += millisecondsElapsed;

    PrunePeers();

    if (s_Flags & _BV(TIME_BROADCAST)) {
        if (s_Flags & _BV(TIME_SCAN)) {
            if (s_Flags & _BV(TIME_INTERVAL)) {
                TerminateInterval(); // make sure stop is received
            }
        } else {
            if (s_Flags & _BV(TIME_INTERVAL)) {
                const int8_t withinListenPeriod = IsInWindow32(s_Mono, NETWORK_RXTX_DURATION, s_StartOfInterval);
                if (withinListenPeriod) {
                    if (s_Stratum != NOT_SYNCED_MARKER) {
                        const uint32_t m1Offset = NETWORK_RXTX_DURATION/3;
                        const uint32_t m2Offset = (UINT32_C(2)*NETWORK_RXTX_DURATION)/3;
                        int8_t third = IsInWindow32(s_Mono, m1Offset, s_StartOfInterval);
                        int8_t twoThird = IsInWindow32(s_Mono, m2Offset, s_StartOfInterval);

                        if (!third && twoThird &&
                            !(s_Flags & _BV(TIME_M1))) {
                            s_Flags |= _BV(TIME_M1);

                            if (s_Callback) {
                                s_Callback(TIME_INT_SEND_RECEIVE_START);
                            }
                        } else if (!twoThird && withinListenPeriod &&
                                   !(s_Flags & _BV(TIME_M2))) {
                            s_Flags |= _BV(TIME_M2);

                            if (s_Callback) {
                                s_Callback(TIME_INT_SEND_RECEIVE_STOP);
                            }
                        }

                        if (!IsInWindow32(s_Mono, TIME_BROADCAST_INTERVAL, s_LastBroadcastTime)) {
                            s_LastBroadcastTime = s_Mono;
                            BroadcastMessage();
                            /*
                            DEBUG_P("Time: %" PRIu32 " broadcast %d seq %u\n",
                                    s_Mono,
                                    (s_LastBroadcastTime - s_StartOfInterval) / (NETWORK_RXTX_DURATION / 256),
                                    s_SequenceNumber);
                                    */
                        }
                    }
                } else {
                    TerminateInterval();
                    DEBUG_P("Time: %" PRIu32 " end of period\n", s_Mono);
                }
                s_TimeToNextInterval -= millisecondsElapsed;
            } else { // not in interval
                if (s_TimeToNextInterval <= millisecondsElapsed) {
                    s_Flags |= _BV(TIME_INTERVAL);
                    uint32_t delayed = millisecondsElapsed - s_TimeToNextInterval;
                    s_StartOfInterval = s_Mono - delayed;
                    s_TimeToNextInterval = NETWORK_PERIOD - delayed;
                    ++s_SequenceNumber;
                    DEBUG_P("Time: %" PRIu32 " start of int %u\n", s_Mono, s_SequenceNumber);
                    if (s_Callback) {
                        s_Callback(TIME_INT_START);
                    }
                } else {
                    s_TimeToNextInterval -= millisecondsElapsed;
                }
            }
        }
    }
}

void
Time_NotifyStartListening(uint8_t scan) {
    s_Flags |= _BV(TIME_LISTENING);
    if (scan) {
        DEBUG_P("Time: %" PRIu32 " scan\n", s_Mono);
        s_Flags |= _BV(TIME_SCAN);
        s_Stratum = NOT_SYNCED_MARKER;
    } else {
        DEBUG_P("Time: %" PRIu32 " listen\n", s_Mono);
    }

    for (Peer* p = s_Peers; p; p = p->Next) {
        p->RequestReceived = 0;
        p->PeerReplyIndex = 0;
        p->PeerReplyCount = 0;
        p->Stratum = NOT_SYNCED_MARKER;
        ClearSourceChainPtr(p->Sources);
    }
}

static
int8_t
IsBetterPeer(Peer* best, Peer* candidate) {
    if (!best) {
        return 1;
    }

    const uint8_t ReplyCountThreshold = 9;

    if (best->PeerReplyCount >= ReplyCountThreshold) {
        if (candidate->PeerReplyCount >= ReplyCountThreshold) {
            if (candidate->Stratum < best->Stratum) {
                return 1;
            } else if (candidate->Stratum == best->Stratum) {
                return !best->RequestReceived && candidate->RequestReceived;
            } else { // candiate stratum is worse
                return 0;
            }
        } else { // candidate not above threshold
            return 0;
        }
    } else { // best not above threshold
        if (candidate->PeerReplyCount >= ReplyCountThreshold) {
            return 1;
        } else { // candidate not above threshold
            if (candidate->Stratum < best->Stratum) {
                return 1;
            } else  if (candidate->Stratum == best->Stratum) {
                return !best->RequestReceived && candidate->RequestReceived;
            } else {
                return 0;
            }
        }
    }
}



void
SetSourceChain(const uint8_t* src, uint8_t myId) {
    uint8_t size = 0;
    for (uint8_t i = 0; i < SOURCE_CHAIN_LENGTH; ++i) {
        if (src[i] == NOT_SYNCED_MARKER) {
            break;
        }

        s_Sources[i] = src[i];
        ++size;
    }

    s_Sources[size++ % SOURCE_CHAIN_LENGTH] = myId;
    s_Sources[size++ % SOURCE_CHAIN_LENGTH] = NOT_SYNCED_MARKER;
}

void
Time_NotifyStopListening() {
    // guard
    if (!(s_Flags & _BV(TIME_LISTENING))) {
        return;
    }

    const uint8_t wasScanning = s_Flags & _BV(TIME_SCAN);
    s_Flags &= ~(_BV(TIME_LISTENING) | _BV(TIME_SCAN));

    if ((s_Flags & (_BV(TIME_SYNC) | _BV(TIME_AUTO_STRATUM))) == (_BV(TIME_SYNC) | _BV(TIME_AUTO_STRATUM))) {

        DEBUG_P("Time: %" PRIu32 " analyze, seq %u\n", s_Mono, s_SequenceNumber);
        const uint8_t previousStratum = s_Stratum;
        const uint8_t myId = Network_GetAddress();
        s_Stratum = NOT_SYNCED_MARKER; // always reset, doubles as flag to abort scan

        ClearSourceChain();
        Peer* best2 = NULL;
        Peer* best1 = NULL;
        Peer* best0 = NULL;

        for (Peer* p = s_Peers; p; p = p->Next) {
#ifdef TIME_DEBUG
            DEBUG_P("Time: peer %02x, stratum %u, seq %u, rc %u, rr? %u, src: ", p->Address, p->Stratum, p->LastSequenceNumber, p->PeerReplyCount, p->RequestReceived);
            for (uint8_t i = 0; i < _countof(p->Sources); ++i) {
                if (p->Sources[i] == NOT_SYNCED_MARKER) {
                    break;
                }

                DEBUG_P("%02x ", p->Sources[i]);
            }
            DEBUG_P("\n");
#endif
            if (p->Stratum != NOT_SYNCED_MARKER) {
                if (p->Stratum < previousStratum) { // do not sync to same level, else partions may sync each other
                    int8_t child = 0;
                    for (uint8_t i = 0; i < _countof(p->Sources); ++i) {
                        if (p->Sources[i] == NOT_SYNCED_MARKER) {
                            break;
                        }

                        if (p->Sources[i] == myId) {
                            child = 1;
                            break;
                        }
                    }
                    if (!child) {
                        if (p->LastSequenceNumber == s_SequenceNumber) {
                            if (p->PeerReplyCount >= 2) {
                                if (IsBetterPeer(best2, p)) {
                                    best2 = p;
                                }
                            } else if (p->PeerReplyCount == 1) {
                                if (IsBetterPeer(best1, p)) {
                                    best1 = p;
                                }
                            }
                        } else {
                            if (IsBetterPeer(best0, p)) {
                                best0 = p;
                            }
                        }
                    }
                }
            }

//            if (wasScanning || !p->RequestReceived) {
//                p->OffsetCount = 0;
//                p->OffsetIndex = 0;
//            }
        }

        // We are scanning add thus are likely to pick up
        // a reply from rpi which is always listening.
        //
        // This can lead the device to believe it is in perfect
        // sync when in fact it is not.
        if (wasScanning) {
            best2 = NULL;
            best1 = NULL;
        }

        if (best2) {
            Peer* best = best2;
            DEBUG_P("Time: best2\n");
            s_Stratum = best->Stratum + 1;
            SetSourceChain(best->Sources, Network_GetAddress());

            //uint8_t oneWayDelay = (best->PeerOneWayDelay[0] + best->PeerOneWayDelay[1]) / 2;
            uint8_t oneWayDelay = (best->PeerTwoWayDelaySum / best->PeerReplyCount) / 2;
            DEBUG_P("Time: one way delay %u\n", oneWayDelay);


            const uint8_t lastIndex = best->PeerReplyIndex ? 0 : 1;
            uint32_t elapsed = (s_Mono - best->PeerReplyTimes[lastIndex]);
            s_TimeToNextInterval = (best->PeerTimesToInterval[lastIndex] - elapsed - oneWayDelay) % (2*NETWORK_PERIOD);
            DEBUG_P("Time: sync to %02x stratum %u -> %u\n", best->Address, best->Stratum, s_Stratum);
            DEBUG_P("Time: tti %" PRIu32 "\n", s_TimeToNextInterval);
#ifdef TIME_DEBUG
            ++s_Counts[2];
#endif
        } else if (best1) {
            Peer* best = best1;
            DEBUG_P("Time: best1\n");
            s_Stratum = best->Stratum + 1;
            SetSourceChain(best->Sources, Network_GetAddress());
            //uint8_t oneWayDelay = best->PeerOneWayDelay[0];
            uint8_t oneWayDelay = best->PeerTwoWayDelaySum / 2;
            DEBUG_P("Time: one way delay %u\n", oneWayDelay);

            uint32_t elapsed = s_Mono - best->PeerReplyTimes[0];
            s_TimeToNextInterval = (best->PeerTimesToInterval[0] - elapsed - oneWayDelay) % (2*NETWORK_PERIOD);
            DEBUG_P("Time: sync to %02x stratum %u -> %u\n", best->Address, best->Stratum, s_Stratum);
            DEBUG_P("Time: tti %" PRIu32 "\n", s_TimeToNextInterval);
#ifdef TIME_DEBUG
            ++s_Counts[1];
#endif
        } else if (best0) {
            Peer* best = best0;
            DEBUG_P("Time: best0\n");
            s_Stratum = best->Stratum + 1;
            SetSourceChain(best->Sources, Network_GetAddress());
            DEBUG_P("Time: latch on to time of %02x stratum %u -> %u\n", best->Address, best->Stratum, s_Stratum);
            uint32_t elapsed = s_Mono - best->TimeOfRequest;
            s_TimeToNextInterval = (best->TimeToIntervalOfRequest - elapsed) % (2*NETWORK_PERIOD);
            DEBUG_P("Time: tti %" PRIu32 "\n", s_TimeToNextInterval);
#ifdef TIME_DEBUG
            ++s_Counts[0];
#endif
        } else {
            DEBUG_P("Time: zip\n");

#ifdef TIME_DEBUG
            ++s_Errors;
#endif
        }

        DEBUG_P("Time: %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 "\n", s_Errors, s_Counts[0], s_Counts[1], s_Counts[2]);
    }
}

void
Time_Process(NetworkPacket* packet) {
    ASSERT_FILE(packet, return, "time");
    ASSERT_FILE(packet->Type == TIME_PACKET_TYPE, return, "time");

    Time_Payload* t = (Time_Payload*)packet->Payload;

<<<<<<< HEAD
=======
//    DEBUG_P("Time: %" PRIu32 " process ori %02x sen %02x\n", s_Mono, t->Originator, t->Sender);



>>>>>>> r2
    if (t->Originator == NETWORK_BROADCAST_ADDRESS) {
        return;
    }

    if (t->Sender == NETWORK_BROADCAST_ADDRESS) {
        return;
    }

    const uint8_t myId = Network_GetAddress();

    if (t->Originator == t->Sender) { // request
        if (t->Originator == myId) {
            // if this ever happens, discard
            return;
        }

        ASSERT_FILE(t->Stratum != NOT_SYNCED_MARKER, return, "time");


//        DEBUG_P("Time: %" PRIu32 " req from %02x\n", s_Mono, t->Sender);

        Peer* peer = GetOrCreatePeer(t->Sender);
        if (peer) {
            peer->LastAwareTime = s_Mono;

            if (!peer->RequestReceived) {
                peer->RequestReceived = 1;
                peer->Stratum = t->Stratum;
                memcpy(peer->Sources, t->Sources, sizeof(peer->Sources));
            }

            peer->TimeOfRequest = s_Mono;
            peer->TimeToIntervalOfRequest = t->TimeToInterval;
        }

        // snoop for any hint of a time to abort scanning asap
        if (NOT_SYNCED_MARKER == s_Stratum) {
            SetSourceChain(t->Sources, myId);
            s_Stratum = t->Stratum + 1; // set this to allow to abort expensive scanning
            DEBUG_P("Time: snooped peer %02x, stratum %u -> %u, tti %" PRIu32 "\n", t->Sender, t->Stratum, s_Stratum, t->TimeToInterval);
        }

        t->TimeToInterval = s_TimeToNextInterval;
        t->Stratum = s_Stratum;
        t->Sender = myId;
        memcpy(t->Sources, s_Sources, sizeof(t->Sources));
        Network_Send(packet);
    } else { // reply
        if (t->Originator != myId) {
            return; // discard
        }

<<<<<<< HEAD


=======
>>>>>>> r2
//        DEBUG_P("Time: %" PRIu32 " rep from %02x\n", s_Mono, t->Sender);

        Peer* peer = GetOrCreatePeer(t->Sender);
        if (!peer) {
            return;
        }

        peer->LastAwareTime = s_Mono;

        // collect reply to own packet
        uint8_t flags = s_Flags;
        if ((flags & _BV(TIME_SYNC)) &&
            !(flags & _BV(TIME_SCAN))) {

            // drop old responses
            if (t->SequenceNumber != s_SequenceNumber) {
                DEBUG_P("Time: drop old reply to seq %u\n", t->SequenceNumber);
                return;
            }

            peer->LastSequenceNumber = s_SequenceNumber;
            peer->PeerReplyTimes[peer->PeerReplyIndex] = s_Mono;
            peer->PeerTimesToInterval[peer->PeerReplyIndex] = t->TimeToInterval;
            //peer->PeerOneWayDelay[peer->PeerReplyIndex] = (s_Mono - t->LocalTime) / 2;
            peer->PeerTwoWayDelaySum += s_Mono - t->LocalTime;
            peer->Stratum = t->Stratum;
            memcpy(peer->Sources, t->Sources, sizeof(peer->Sources));


            ++peer->PeerReplyIndex;
            peer->PeerReplyIndex %= 2;
            if (peer->PeerReplyCount < 255) {
                ++peer->PeerReplyCount;
            }

//            DEBUG_P("Time: peer %02x count %u\n", t->Sender, peer->PeerReplyCount);
        }
    }
}

void
Time_Sync(uint8_t enable) {
    if (enable) {
        s_Flags |= _BV(TIME_SYNC);
    } else {
        s_Flags &= ~_BV(TIME_SYNC);
    }
}

void
Time_Broadcast(uint8_t enable) {
    if (enable) {
        s_Flags |= _BV(TIME_BROADCAST);
    } else {
        s_Flags &= ~_BV(TIME_BROADCAST);
    }
}

uint32_t
Time_TimeToNextInterval() {
    return s_TimeToNextInterval;
}

void
Time_SetStratum(int16_t stratum) {
    if (stratum < 0) {
        s_Flags |= _BV(TIME_AUTO_STRATUM);
        s_Stratum = NOT_SYNCED_MARKER;
    } else {
        s_Flags &= ~_BV(TIME_AUTO_STRATUM);
        s_Stratum = stratum;
    }
}

void
Time_SetSyncWindowCallback(Time_SyncWindowCallback callback) {
    s_Callback = callback;
}
<<<<<<< HEAD
=======

void
Time_BroadcastTime() {
    BroadcastMessage();
}
>>>>>>> r2
