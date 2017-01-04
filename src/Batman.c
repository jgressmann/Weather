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


#include <stdlib.h>
#include <string.h>

#include "Batman.h"
#include "Network.h"
#include "Misc.h"
#include "Time.h"
#include "Debug.h"

#ifndef BATMAN_DEBUG
#   undef DEBUG
#   define DEBUG(...)
#   undef DEBUG_P
#   define DEBUG_P(...)
#endif


/* Batman packet adapted for RF24
 *
 * https://tools.ietf.org/html/draft-wunderlich-openmesh-manet-routing-00
 *
 * Skip version field, no gw stuff, no hna
 *
 */
typedef struct {
    uint8_t Sender;
    uint8_t Originator;
    uint8_t IsDirectLink;
    uint8_t UniDirectional;
    uint16_t SequenceNumber;
} Batman_OGM_Payload;


#define BATMAN_ORIGINATOR_INVERVAL NETWORK_PERIOD /* must be the same as the network interval */
#define BATMAN_WINDOW_SIZE 16
#define BATMAN_BIDIR_LINK_TIMEOUT BATMAN_WINDOW_SIZE / 2
#define BATMAN_PURGE_TIMEOUT (10ul*BATMAN_WINDOW_SIZE*BATMAN_ORIGINATOR_INVERVAL)



struct _Batman_Originator;
typedef struct _Batman_Neighbor {
    struct _Batman_Neighbor* Next;
    uint32_t LastValidTime;
    uint8_t LastTTL;
    uint8_t Address; // back-pointer to originator
    uint16_t OgmsReceivedInWindow;
} Batman_Neighbor;

typedef struct _Batman_Originator {
    struct _Batman_Originator* Next;
    uint8_t Address;
    uint32_t LastAwareTime;
    uint16_t BiDirLinkSequenceNumber;
    uint16_t CurrentSequenceNumber;
    Batman_Neighbor* Neighbors;
} Batman_Originator;

typedef struct _Batman_Originator_List_Node {
    struct _Batman_Originator_List_Node* Next;
    Batman_Originator* Originator;
} Batman_Originator_List_Node;




static Batman_Originator* s_Originators NOINIT;
static uint16_t s_SequenceNumber NOINIT;
static uint32_t s_LastOgmBroadcastTime NOINIT;
#ifdef BATMAN_DEBUG
static uint8_t s_Dst NOINIT;
#endif

void
Batman_Init() {
    s_Originators = NULL;
    s_LastOgmBroadcastTime = Time_Now() - BATMAN_ORIGINATOR_INVERVAL;
    DEBUG_P("Batman: init\n");
}

static
void
FreeOriginator(Batman_Originator* o) {
    while (o->Neighbors) {
        Batman_Neighbor* n = o->Neighbors;
        o->Neighbors = o->Neighbors->Next;
        free(n);
    }
    free(o);
}

static
void
PruneTimedOutNeigbors(Batman_Originator* owner, uint32_t time) {
    Batman_Neighbor* newHead = NULL;
    while (owner->Neighbors) {
        Batman_Neighbor* n = owner->Neighbors;
        owner->Neighbors = owner->Neighbors->Next;
        if (IsInWindow32(time, BATMAN_PURGE_TIMEOUT, n->LastValidTime)) {
            n->Next = newHead;
            newHead = n;
        } else {
            DEBUG_P("Batman: prune neighbor %#02x of originator %#02x\n", n->Address, owner->Address);
            free(n);
        }
    }

    owner->Neighbors = newHead;
}

void
Batman_Uninit() {
    DEBUG_P("Batman: uninit\n");
    while (s_Originators) {
        Batman_Originator* next = s_Originators->Next;
        FreeOriginator(s_Originators);
        s_Originators = next;
    }
}


static
Batman_Originator*
FindOriginator(uint8_t id) {
    for (Batman_Originator* o = s_Originators; o; o = o->Next) {
        if (o->Address == id) {
            return o;
        }
    }
    return NULL;
}

static
Batman_Originator*
GetOrCreateOriginator(uint8_t id) {
    Batman_Originator* result = FindOriginator(id);
    if (!result) {
        DEBUG_P("Batman: create originator %#02x\n", id);
        result = (Batman_Originator*)malloc(sizeof(*result));
        if (result) {
            memset(result, 0, sizeof(*result));
            result->Address = id;
            result->Next = s_Originators;
            s_Originators = result;
        }
    }
    return result;
}

static
Batman_Neighbor*
FindNeighbor(Batman_Neighbor* head, uint8_t id) {
    //DEBUG_P("Batman: Find neighbor %02x: ", id);
    while (head) {
        if (head->Address == id) {
            break;
        }

//        DEBUG_P("%02x ", head->Address);

        head = head->Next;
    }

//    DEBUG_P("%s\n", head ? "SUCCESS" : "FAIL");

    return head;
}

static
Batman_Neighbor*
GetOrCreateNeighbor(Batman_Originator* owner, uint8_t id) {
    Batman_Neighbor* result = FindNeighbor(owner->Neighbors, id);
    if (!result) {
        DEBUG_P("Batman: create neighbor %#02x of originator %#02x\n", id, owner->Address);
        result = (Batman_Neighbor*)malloc(sizeof(*result));
        if (result) {
            memset(result, 0, sizeof(*result));
            result->Address = id;
            result->Next = owner->Neighbors;
            owner->Neighbors = result;
        }
    }
    return result;
}



static
void
Rebroadcast(NetworkPacket* packet, int8_t receivedViaBiDirLink) {
    if (packet->TTL <= 1) {
        return;
    }

    --packet->TTL;
    Batman_OGM_Payload* ogm = (Batman_OGM_Payload*)&packet->Payload;
    ogm->UniDirectional = !receivedViaBiDirLink;
    ogm->IsDirectLink = ogm->Originator == ogm->Sender;

#ifdef BATMAN_DEBUG
    static uint8_t lastOri, lastSen, lastSeq;
    if (lastOri != ogm->Originator ||
        lastSen != ogm->Sender ||
        lastSeq != s_SequenceNumber) {
        lastOri = ogm->Originator;
        lastSen = ogm->Sender;
        lastSeq = s_SequenceNumber;
        DEBUG_P("Batman: R ori %02x sen %02x dl %d\n", ogm->Originator, ogm->Sender, ogm->IsDirectLink);
    }
#endif
    ogm->Sender = Network_GetAddress();
    Network_Send(packet);
}

static
uint8_t
Route(uint8_t destination, uint32_t time) {
    //DEBUG_P("Batman: Route lookup for %02x\n", destination);
    uint8_t neighborId = NETWORK_BROADCAST_ADDRESS; // broadcast
    Batman_Originator* o = FindOriginator(destination);
    if (o) {
        Batman_Neighbor* best = NULL;
        uint8_t bestOgmsReceived = 0;
        for (Batman_Neighbor* n = o->Neighbors; n; n = n->Next) {
            //DEBUG_P("Batman: Looking at neighbor %02x ... ", n->Address);
            uint8_t ogmsReceived = __builtin_popcount(n->OgmsReceivedInWindow);
            if (IsInWindow32(time, BATMAN_PURGE_TIMEOUT, n->LastValidTime) && ogmsReceived) {
                //DEBUG_P("eligible\n");
                if (!best || bestOgmsReceived < ogmsReceived) {
                    best = n;
                    bestOgmsReceived = ogmsReceived;
                }
            } else {
                //DEBUG_P("obsolete\n");
            }
        }

        if (best) {
            neighborId = best->Address;
        }
    }
#ifdef BATMAN_DEBUG
    if (destination != s_Dst) {
        s_Dst = destination;
        DEBUG_P("Batman: %02x -> %02x\n", destination, neighborId);
    }
#endif

    return neighborId;
}


uint8_t
Batman_Route(uint8_t destination) {
    if (destination == Network_GetAddress()) {
        return destination;
    }

    return Route(destination, Time_Now());
}

static
void
BroadcastOgm() {
    //DEBUG_P("Batman: broadcast OGM %u\n", s_Batman_Sequence_Number);
    NetworkPacket packet;
    Batman_OGM_Payload* ogm = (Batman_OGM_Payload*)&packet.Payload;
    ogm->Sender = Network_GetAddress();
    ogm->Originator = Network_GetAddress();
    //packet.Size = sizeof(Batman_OGM_Payload);
    packet.TTL = Network_GetTtl();
    packet.Type = BATMAN_PACKET_TYPE;
    ogm->IsDirectLink = 0;
    ogm->UniDirectional = 0;
    ogm->SequenceNumber = s_SequenceNumber;

    Network_Send(&packet);
}

static
void
PruneTimedOutOriginators(uint32_t time) {
    Batman_Originator* newHead = NULL;
    while (s_Originators) {
        Batman_Originator* o = s_Originators;
        s_Originators = s_Originators->Next;
        if (IsInWindow32(time, BATMAN_PURGE_TIMEOUT, o->LastAwareTime)) {
            PruneTimedOutNeigbors(o, time);
            o->Next = newHead;
            newHead = o;
        } else {
            DEBUG_P("Batman: prune originator %#02x\n", o->Address);
            FreeOriginator(o);
        }
    }

    s_Originators = newHead;
}

void
Batman_Process(NetworkPacket* packet) {
    Batman_OGM_Payload* ogm = (Batman_OGM_Payload*)&packet->Payload;
    //DEBUG_P("Batman: process Ori %02x Sen %02x Seq %u, DL %d, Uni %d\n", ogm->Originator, ogm->Sender, ogm->SequenceNumber, ogm->IsDirectLink, ogm->UniDirectional);

    const uint8_t myId = Network_GetAddress();
    const uint32_t now = Time_Now();

    PruneTimedOutOriginators(now);

    if (ogm->Sender == myId) {
        return; // as per section 5.2. number 2
    }

    if (ogm->Sender == NETWORK_BROADCAST_ADDRESS) { // broadcast
        return; // as per section 5.2. number 3
    }

    Batman_Originator* sender = GetOrCreateOriginator(ogm->Sender);
    if (!sender) {
        return;
    }

    sender->LastAwareTime = now;

    if (ogm->Originator == myId) {
        //DEBUG_P("Batman: Received own packet back (packet seq %u, current seq %u)\n", ogm->SequenceNumber, s_Batman_Sequence_Number);
        // 5.3.  Bidirectional Link Check
        // recevied via interface sent is trivially true
        if (ogm->IsDirectLink && s_SequenceNumber == ogm->SequenceNumber) {
            sender->BiDirLinkSequenceNumber = ogm->SequenceNumber;
            //DEBUG_P("Batman: Bidir update sen %02x to seq %u\n", ogm->Sender, sender->BiDirLinkSequenceNumber);
        }
        return; // as per section 5.2. number 4
    }

    Batman_Originator* originator = GetOrCreateOriginator(ogm->Originator);
    if (!originator) {
        return;  // out of memory
    }

    originator->LastAwareTime = now;

    if (ogm->UniDirectional) {
        DEBUG_P("Batman: drop uni dir packet\n");
        return; // as per section 5.2. number 5
    }

    int8_t receivedViaBiDirLink = IsInWindow16(s_SequenceNumber, BATMAN_BIDIR_LINK_TIMEOUT, sender->BiDirLinkSequenceNumber);
    //DEBUG_P("Batman: Sen bi-dir seq %u my seq %u window %u, recv via bi-dir %d\n", sender->BiDirLinkSequenceNumber, s_Batman_Sequence_Number, BATMAN_BIDIR_LINK_TIMEOUT, receivedViaBiDirLink);

    Batman_Neighbor* neighbor = NULL;
    int8_t duplicate = 0;

    if (receivedViaBiDirLink) {
        // Section 5.4. processing, neighbor ranking
        neighbor = GetOrCreateNeighbor(originator, ogm->Sender);
        if (neighbor) {
            neighbor->LastValidTime = now;
            //DEBUG_P("Batman: ori seq %u ogm %u, window %d\n", originator->CurrentSequenceNumber, ogm->SequenceNumber, BATMAN_WINDOW_SIZE);
            if (IsInWindow16(originator->CurrentSequenceNumber, BATMAN_WINDOW_SIZE, ogm->SequenceNumber)) {
                uint8_t index = originator->CurrentSequenceNumber - ogm->SequenceNumber;
                uint16_t bit = 1<<index;
                duplicate = (neighbor->OgmsReceivedInWindow & bit) == bit;
                neighbor->OgmsReceivedInWindow |= bit;
            } else {
                uint16_t diff = ogm->SequenceNumber - originator->CurrentSequenceNumber;
                if (diff > 1) {
                    DEBUG_P("Batman: missed %u packets\n", diff);
                }
                if (diff < BATMAN_WINDOW_SIZE) {
                    neighbor->OgmsReceivedInWindow <<= diff;
                } else {
                    neighbor->OgmsReceivedInWindow = 0;
                }

                // update sequence number
                originator->CurrentSequenceNumber = ogm->SequenceNumber;

                // this new OGM
                neighbor->OgmsReceivedInWindow |= 1;
                neighbor->LastTTL = packet->TTL;
            }

            //DEBUG_P("Batman: Ori %02x via nei %02x rank %u\n", ogm->Originator, ogm->Sender, neighbor->OgmsReceivedInWindow);
        }
    }

    // as per section 5.2. number 7
    int8_t rebroadcast = ogm->Sender == ogm->Originator;
    if (!rebroadcast) {
        if (receivedViaBiDirLink &&
            Route(ogm->Originator, now) == ogm->Sender &&
            neighbor) {
            rebroadcast = packet->TTL == neighbor->LastTTL || !duplicate;
        }
    }

    if (rebroadcast) {
        // Section 5.5 rebroadcast
        Rebroadcast(packet, receivedViaBiDirLink);
    }
}

void
Batman_Update() {
    const uint32_t now = Time_Now();

    PruneTimedOutOriginators(now);

    if (now - s_LastOgmBroadcastTime >= BATMAN_ORIGINATOR_INVERVAL) {
        s_LastOgmBroadcastTime = now;
        ++s_SequenceNumber;
        DEBUG_P("Batman: OGM %u\n", s_SequenceNumber);
        BroadcastOgm();
    }
}

void
Batman_Broadcast() {
    BroadcastOgm();
}
