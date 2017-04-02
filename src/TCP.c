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

#include <stdlib.h>
#include <string.h>


#include "Batman.h"
#include "Misc.h"
#include "Time.h"
#include "TCP.h"
#include "Debug.h"


#if AVR
#   include <avr/sfr_defs.h>
#else
#   define _BV(x) (1<<(x))
#endif

#ifndef TCP_DEBUG
#   undef DEBUG
#   define DEBUG(...)
#   undef DEBUG_P
#   define DEBUG_P(...)
#endif

#define DEBUG_MALLOC_FAIL DEBUG_P("TCP: out of mem line %d\n", __LINE__)

#define TCP_PACKET_RECEIVED 0

// Mini TCP implementation based on https://tools.ietf.org/html/rfc793
// tailored for short send/receive intervals


#ifdef AVR
#   include <util/crc16.h>
#else
static
uint8_t
_crc8_ccitt_update (uint8_t inCrc, uint8_t inData) {
    uint8_t   i;
    uint8_t   data;

    data = inCrc ^ inData;

    for ( i = 0; i < 8; i++ ) {
        if (( data & 0x80 ) != 0 ) {
            data <<= 1;
            data ^= 0x07;
        } else {
            data <<= 1;
        }
    }
    return data;
}
#endif



typedef struct _UnacknowledgedPacket {
    struct _UnacknowledgedPacket* Next;
    uint32_t TimeSent;
    NetworkPacket Packet;
    uint8_t Count;
} UnacknowledgedPacket;

typedef struct _UndeliveredPacket {
    struct _UndeliveredPacket* Next;
    uint8_t SequenceNumber;
    uint8_t Sender;
    uint8_t Size;
    uint8_t Data[0];
} UndeliveredPacket;

typedef struct _PeerData {
    struct _PeerData* Next;
    UnacknowledgedPacket* Unacknowledged;
    UndeliveredPacket* Undelivered;
    uint8_t SendSequenceNumber;
    uint8_t ReceivedSequenceNumber;
    uint8_t ReceiveWindow;
    uint8_t AckSequenceNumber;
    uint8_t Address;
    uint8_t Flags;
} PeerData;

TCP_DataReceivedCallback s_DataReceivedCallback NOINIT;
PeerData* s_Peers NOINIT;


#define TCP_ACK_TIMEOUT 256
#define TCP_RECV_WINDOW 8
#define TCP_ACK_WINDOW (UINT8_C(1)<<7)

typedef struct {
    uint8_t Seq;
    uint8_t Sender;
    uint8_t Via;
    uint8_t Destination;
    uint8_t Ack     : 1;
    uint8_t Size    : 5;
    uint8_t Crc;
    uint8_t Data[TCP_PAYLOAD_SIZE];
} TCP_Payload;



static
void
StoreChecksum(TCP_Payload* tcp) {
    ASSERT_FILE(tcp, return, "tcp");

    tcp->Crc = 0;
    const uint8_t* ptr = (uint8_t*)tcp;
    uint8_t crc = 0xff;
    for (uint8_t i = 0; i < sizeof(*tcp); ++i) {
        crc = _crc8_ccitt_update(crc, ptr[i]);
    }
    tcp->Crc = crc;
}

static
int8_t
IsChecksumValid(TCP_Payload* tcp) {
    ASSERT_FILE(tcp, return 0, "tcp");
    uint8_t loaded = tcp->Crc;
    StoreChecksum(tcp);
    uint8_t computed = tcp->Crc;
    tcp->Crc = loaded;
    return loaded == computed;
}

static
PeerData*
FindPeer(uint8_t id) {
    for (PeerData* o = s_Peers; o; o = o->Next) {
        if (o->Address == id) {
            return o;
        }
    }
    return NULL;
}

static
PeerData*
GetOrCreatePeer(uint8_t address) {
    PeerData* result = FindPeer(address);
    if (!result) {
        DEBUG_P("TCP: create peer %#02x\n", address);
        result = (PeerData*)malloc(sizeof(*result));
        if (result) {
            memset(result, 0, sizeof(*result));
            result->Address = address;
            result->Next = s_Peers;
            s_Peers = result;
        } else {
            DEBUG_MALLOC_FAIL;
        }
    }
    return result;
}

static
void
Clear() {
    while (s_Peers) {
        PeerData* peer = s_Peers;
        s_Peers = s_Peers->Next;
        while (peer->Unacknowledged) {
            UnacknowledgedPacket* entry = peer->Unacknowledged;
            peer->Unacknowledged = peer->Unacknowledged->Next;
            free(entry);
        }
        while (peer->Undelivered) {
            UndeliveredPacket* entry = peer->Undelivered;
            peer->Undelivered = peer->Undelivered->Next;
            free(entry);
        }
        free(peer);
    }
}

void
TCP_Init() {
    s_Peers = NULL;
    s_DataReceivedCallback = NULL;
    DEBUG_P("TCP: init\n");
}

void
TCP_Uninit() {
    Clear();
    DEBUG_P("TCP: uninit\n");
}


void
TCP_Update() {

//    uint32_t now = s_TimeCallback();
    uint32_t now = Time_Now();
    for (PeerData* peer = s_Peers; peer; peer = peer->Next) {
        UnacknowledgedPacket** previous = &peer->Unacknowledged;
        for (UnacknowledgedPacket* entry = *previous; entry; previous = &entry->Next, entry = entry->Next) {
            if (!IsInWindow32(now, TCP_ACK_TIMEOUT, entry->TimeSent)) {
                entry->TimeSent = now;
                ++entry->Count;
                TCP_Payload* tcp = (TCP_Payload*)&entry->Packet.Payload;
                uint8_t newVia = Batman_Route(tcp->Destination); // routing info may have changed
                if (newVia != tcp->Via) {
                    tcp->Via = newVia;
                    StoreChecksum(tcp);
                }
                Network_Send(&entry->Packet);
                DEBUG_P("TCP: rt %u to %02x via %02x (%u)\n", tcp->Seq, tcp->Destination, tcp->Via, entry->Count);
            }
        }
    }
}


void
TCP_Process(NetworkPacket* packet) {
    ASSERT_FILE(packet, return, "tcp");
    ASSERT_FILE(packet->Type == TCP_PACKET_TYPE, return, "tcp");

    TCP_Payload* tcp = (TCP_Payload*)packet->Payload;


    // drop packets that don't have a proper sender
    if (tcp->Sender == NETWORK_BROADCAST_ADDRESS) {
        return;
    }

    const uint8_t myid = Network_GetAddress();
    // drop packets that looped back to me
    if (tcp->Sender == myid) {
        return;
    }

    // drop packets with invalid checksum
    if (!IsChecksumValid(tcp)) {
        DEBUG_P("TCP: checksum failure, from %02x via %02x to %02x\n", tcp->Sender, tcp->Via, tcp->Destination);
        return;
    }

    if (tcp->Destination == myid) {
        PeerData* sender = GetOrCreatePeer(tcp->Sender);
        if (!sender) {
            return;
        }

        if (tcp->Ack) {
            UnacknowledgedPacket* newHead = NULL;
            while (sender->Unacknowledged) {
                UnacknowledgedPacket* uap = sender->Unacknowledged;
                sender->Unacknowledged =  sender->Unacknowledged->Next;

                TCP_Payload* unAckPayload = (TCP_Payload*)&uap->Packet.Payload;
                if (IsInWindow8(tcp->Ack, TCP_ACK_WINDOW, unAckPayload->Seq)) {
//                if (unAckPayload->Seq == tcp->Seq) {
                    DEBUG_P("TCP: recv ack for seq %u sent to %02x\n", unAckPayload->Seq, tcp->Sender);
                    free(uap);
                } else {
                    uap->Next = newHead;
                    newHead = uap;
                }
            }
            sender->Unacknowledged = newHead;
        } else {
            int8_t ack = 0;
//            int8_t deliver = 0;
            if (!(sender->Flags & _BV(TCP_PACKET_RECEIVED))) {
                sender->Flags |= _BV(TCP_PACKET_RECEIVED);
                sender->ReceivedSequenceNumber = tcp->Seq;
                sender->AckSequenceNumber = tcp->Seq;
                sender->ReceiveWindow = 0;
                DEBUG_P("TCP: sync recv seq to %u for %02x\n", tcp->Seq, tcp->Sender);
            }

            if (IsInWindow8(sender->ReceivedSequenceNumber + (TCP_RECV_WINDOW - 1), TCP_RECV_WINDOW, tcp->Seq)) {
                ack = 1;
                uint8_t index = sender->ReceivedSequenceNumber - tcp->Seq;
                DEBUG_P("TCP: recv window %u index %d from %02x\n", sender->ReceivedSequenceNumber, index, tcp->Sender);
                uint8_t bit = UINT8_C(1) << index;
                if (sender->ReceiveWindow & bit) {
                    // nothing to do, already seen
                    DEBUG_P("TCP: dup seq %u from %02x\n", tcp->Seq, tcp->Sender);
                } else {
                    DEBUG_P("TCP: queue %u bytes from %02x for delivery\n", tcp->Size, tcp->Sender);
                    UndeliveredPacket* up = (UndeliveredPacket*)malloc(sizeof(*up) + tcp->Size);
                    if (up) {
                        sender->ReceiveWindow |= bit;

                        // update ack seq number
                        sender->AckSequenceNumber = sender->ReceivedSequenceNumber;
                        for (uint8_t i = sender->ReceiveWindow; i & 1; i >>= 1) {
                            ++sender->AckSequenceNumber;
                        }

                        up->Sender = tcp->Sender;
                        up->Size = tcp->Size;
                        up->SequenceNumber = tcp->Seq;
                        memcpy(up->Data, tcp->Data, up->Size);
                        up->Next = sender->Undelivered;
                        sender->Undelivered = up;
//                        deliver = 1;

                    } else {
                        DEBUG_MALLOC_FAIL;
                        ack = 0;
                    }
                }

            } else if (IsInWindow8(sender->AckSequenceNumber, TCP_ACK_WINDOW, tcp->Seq)) {
                ack = 1;
            } else {
                DEBUG_P("TCP: oob packet %u seq %u for %02x\n", tcp->Seq, sender->ReceivedSequenceNumber, tcp->Sender);
            }

            if (ack) {
                tcp->Seq = sender->AckSequenceNumber - 1;
                tcp->Ack = 1;
                tcp->Destination = tcp->Sender;
                tcp->Sender = myid;
                tcp->Via = Batman_Route(tcp->Destination);
                packet->TTL = Network_GetTtl();

                DEBUG_P("TCP: send ack till %u to %02x via %02x\n", tcp->Seq, tcp->Destination, tcp->Via);

                StoreChecksum(tcp);
                Network_Send(packet);
            }

            while (sender->ReceiveWindow & 1) {
                ASSERT_FILE(sender->Undelivered, return, "tcp");

                UndeliveredPacket* oldest = NULL;
                UndeliveredPacket* newHead = NULL;
                while (sender->Undelivered) {
                    UndeliveredPacket* up = sender->Undelivered;
                    sender->Undelivered = sender->Undelivered->Next;
                    if (up->SequenceNumber == sender->ReceivedSequenceNumber) {
                        oldest = up;
                    } else {
                        up->Next = newHead;
                        newHead = up->Next;
                    }
                }

                sender->Undelivered = newHead;
                ASSERT_FILE(oldest, return, "tcp");

                if (s_DataReceivedCallback) {
                    DEBUG_P("TCP: deliver packet %u from %02x\n", oldest->SequenceNumber, oldest->Sender);
                    s_DataReceivedCallback(oldest->Sender, oldest->Data, oldest->Size);
                }

                DEBUG_P("TCP: purge packet %u from %02x\n", oldest->SequenceNumber, oldest->Sender);
                free(oldest);

                sender->ReceiveWindow >>= 1;
                ++sender->ReceivedSequenceNumber;
            }
        }
    } else if (tcp->Destination != NETWORK_BROADCAST_ADDRESS &&
               (tcp->Via == NETWORK_BROADCAST_ADDRESS || tcp->Via == myid)) {
        if (packet->TTL <= 1) {
            DEBUG_P("TCP: tll death from %02x via %02x to %02x\n", tcp->Sender, tcp->Via, tcp->Destination);
        } else {
            const uint8_t via = Batman_Route(tcp->Destination);
            if (via != tcp->Sender) { // don't send a packet back the way it just came
                --packet->TTL;
                tcp->Via = via;

                DEBUG_P("TCP: fw from %02x via %02x to %02x ttl %u\n", tcp->Sender, tcp->Via, tcp->Destination, packet->TTL);

                StoreChecksum(tcp);
                Network_Send(packet);
            }
        }
    } else {
        // drop
    }
}

void
TCP_Send(uint8_t destination, const uint8_t *ptr, uint8_t size) {
    ASSERT_FILE(ptr, return, "tcp");
    ASSERT_FILE(size, return, "tcp");
    ASSERT_FILE(size <= TCP_PAYLOAD_SIZE, return, "tcp");

    const uint8_t myid = Network_GetAddress();
    if (myid == destination) {
        if (s_DataReceivedCallback) {
            s_DataReceivedCallback(myid, ptr, size);
        }
    } else {
        PeerData* peer = GetOrCreatePeer(destination);
        if (!peer) {
            return;
        }

        UnacknowledgedPacket* uap = (UnacknowledgedPacket*)malloc(sizeof(*uap));
        if (!uap) {
            DEBUG_MALLOC_FAIL;
            return;
        }


        uap->TimeSent = Time_Now();
        uap->Count = 0;
        uap->Next = peer->Unacknowledged;
        peer->Unacknowledged = uap;


        NetworkPacket* packet = &uap->Packet;
        packet->TTL = Network_GetTtl();
        packet->Type = TCP_PACKET_TYPE;

        TCP_Payload* tcp = (TCP_Payload*)packet->Payload;
        tcp->Ack = 0;
        tcp->Sender = myid;
        tcp->Seq = peer->SendSequenceNumber++;
        tcp->Size = size;
        tcp->Destination = destination;
        tcp->Via = Batman_Route(tcp->Destination);

        memcpy(tcp->Data, ptr, size);

        StoreChecksum(tcp);
        Network_Send(packet);
        DEBUG_P("TCP: seq %u send to %02x via %02x\n", tcp->Seq, tcp->Destination, tcp->Via);
    }
}

void
TCP_Purge() {
    DEBUG_P("TCP: purge\n");
    Clear();
}

void
TCP_SetDataReceivedCallback(TCP_DataReceivedCallback callback) {
    s_DataReceivedCallback = callback;
}

void
TCP_Decode(NetworkPacket* packet, uint8_t* sender, uint8_t* destination, uint8_t** ptr, uint8_t* bytes) {
    ASSERT_FILE(packet, return, "tcp");
    ASSERT_FILE(sender, return, "tcp");
    ASSERT_FILE(destination, return, "tcp");
    ASSERT_FILE(ptr, return, "tcp");
    ASSERT_FILE(bytes, return, "tcp");
    TCP_Payload* payload = (TCP_Payload*)packet->Payload;
    *sender = payload->Sender;
    *destination = payload->Destination;
    *ptr = payload->Data;
    *bytes = payload->Size;
}
