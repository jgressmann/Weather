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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>

#include <set>
#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <toe/cmdlopt.h>
#include <linuxapi/linuxapi.h>

#include "../../Network.h"
#include "../../Batman.h"
#include "../../Time.h"
#include "../../TCP.h"

#include "Globals.h"

#ifndef UINT64_C
#   define UINT64_C(x) static_cast<uint64_t>(x)
#endif


#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)
#define ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)
#define LOG(...) fprintf(stdout, __VA_ARGS__)

#if 1
#   define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#   define DEBUG(...)
#endif


static void EPollPacketRouterHandler(void* ctx, epoll_event* ev);
static void EPollTimerHandler(void *ctx, epoll_event *ev);

static void EPollAcceptHandler(void* ctx, epoll_event* ev);
static void EPollConnectionDataHandler(void* ctx, epoll_event* ev);


static sem_t s_Shutdown;

static
void
Shutdown() {
    sem_post(&s_Shutdown);
}

static
void
SignalHandler(int) {
    Shutdown();
}


template<typename T>
static
int
UnsignedParser(const char* arg, T& value) {
    char* end = NULL;
    value = (T)strtoul(arg, &end, 10);
    if (!end || end == arg) {
        ERROR("Argument '%s' could not be converted to unsigned int\n", arg);
        return -1;
    }

    return 0;
}

static const cmdlopt_arg s_Dummy_Arg[] = {
    CMDLOPT_ARGUMENT_TERMINATOR
};

static const char* s_NetworkSocketPath = RF24_NETWORK_SOCKET_PATH;
static
int
NetworkSocketPath_Parser(void*, char* arg) {
    s_NetworkSocketPath = arg;
    return 0;
}

static const char* s_PacketRouterSocketPath = RF24_PACKET_ROUTER_SOCKET_PATH;
static
int
RouterSocketPath_Parser(void*, char* arg) {
    s_PacketRouterSocketPath = arg;
    return 0;
}

static uint8_t s_Network_Address = 0xfe;
static
int
Id_Parser(void*, char* arg) {
    int error = UnsignedParser(arg, s_Network_Address);
    if (!error) {
        if (s_Network_Address == 0xff) {
            ERROR("Invalid id 0xff (= broadcast id)\n");
            error = -1;
        }
    }

    return error;
}


static bool EnabledParser(const char* str) {
    return  !(strcasecmp(str, "no") == 0 ||
            strcasecmp(str, "0") == 0 ||
            strcasecmp(str, "false") == 0);
}

static bool s_Batman_Enabled = true;
static
int
BatmanEnabled_Parser(void*, char* arg) {
    s_Batman_Enabled = EnabledParser(arg);
    return 0;
}

static bool s_Tcp_Enabled = true;
static
int
TcpEnabled_Parser(void*, char* arg) {
    s_Tcp_Enabled = EnabledParser(arg);
    return 0;
}

static bool s_Time_Enabled = true;
static
int
TimeEnabled_Parser(void*, char* arg) {
    s_Time_Enabled = EnabledParser(arg);
    return 0;
}


static uint8_t s_Time_Stratum = 0;
static
int
TimeStratum_Parser(void*, char* arg) {
    return UnsignedParser(arg, s_Time_Stratum);
}

static uint32_t s_Time_Tick_Millis = 1000;
static
int
TimeTick_Parser(void*, char* arg) {
    return UnsignedParser(arg, s_Time_Tick_Millis);
}

<<<<<<< HEAD
=======
static bool s_Time_BroadcastOnTick = true;
static
int
TimeBroadcastOnTick_Parser(void*, char* arg) {
    s_Time_BroadcastOnTick = EnabledParser(arg);
    return 0;
}

static bool s_Time_PrintTti = false;
static
int
TimePrintTti_Parser(void*, char* arg) {
    s_Time_PrintTti = EnabledParser(arg);
    return 0;
}

>>>>>>> r2
static uint8_t s_Network_Ttl = 0xff;
static
int
NetworkTtl_Parser(void*, char* arg) {
    return UnsignedParser(arg, s_Network_Ttl);
}

static const cmdlopt_opt s_Options[] = {
    { "bat-enable", "Enables Batman. Defaults to yes.", 'b', 0x103, s_Dummy_Arg, BatmanEnabled_Parser },
    { "net-id", "Id to use for Network. Defauls to 0xfe (254).", 'i', 0x100, s_Dummy_Arg, Id_Parser },
    { "net-socket-path", "Path to UNIX socket. Defaults to " RF24_NETWORK_SOCKET_PATH, 0, 0x102, s_Dummy_Arg, NetworkSocketPath_Parser },
    { "net-ttl", "Packet time to live (TTL). Defaults to 255.", 0, 0x104, s_Dummy_Arg, NetworkTtl_Parser },
    { "packet-router-socket-path", "Path to UNIX socket. Defaults to " RF24_PACKET_ROUTER_SOCKET_PATH, 0, 0x200, s_Dummy_Arg, RouterSocketPath_Parser },
    { "tcp-enable", "Enables TCP. Defaults to yes.", 0, 0x300, s_Dummy_Arg, TcpEnabled_Parser },
    { "time-enable", "Enables time. Defaults to yes.", 0, 0x110, s_Dummy_Arg, TimeEnabled_Parser },
    { "time-stratum", "Stratum of time. Lower values mean better clock. Defaults to 0.", 0, 0x111, s_Dummy_Arg, TimeStratum_Parser },
    { "time-tick", "Interval between time ticks. Defaults to 1000 [ms].", 0, 0x112, s_Dummy_Arg, TimeTick_Parser },
<<<<<<< HEAD

=======
    { "time-broadcast-on-tick", "Broadcast the time on tick. Defaults to true.", 0, 0x113, s_Dummy_Arg, TimeBroadcastOnTick_Parser },
    { "time-tti", "Print time-to-interval (tti) periodically.", 0, 0x114, s_Dummy_Arg, TimePrintTti_Parser },
>>>>>>> r2
    CMDLOPT_COMMON_OPTIONS,
    CMDLOPT_OPTION_TERMINATOR
};


static int s_TimerFd = -1;
static int s_PacketRouterSocketFD = -1;
static uint64_t s_LastIterationsTimestamp;
<<<<<<< HEAD
=======
static uint64_t s_LastTimeBroadcastTimestamp;
>>>>>>> r2
typedef std::set<int> HandleSet;
static HandleSet s_TcpConnections;
static HandleSet s_Connections;
static int s_In_SendReceive_Window;
static
void
NetworkSendCallback(NetworkPacket* packet) {
<<<<<<< HEAD
=======
//    DEBUG("Send to packet router\n");
>>>>>>> r2
    write(s_PacketRouterSocketFD, packet, sizeof(*packet));
}

static
void
TcpDataReceived(uint8_t sender, const uint8_t* payload, uint8_t size) {
    HandleSet::const_iterator it = s_TcpConnections.begin();
    HandleSet::const_iterator end = s_TcpConnections.end();

    for (; it != end; ++it) {
        write(*it, &sender, 1);
        write(*it, &size, 1);
        write(*it, payload, size);
    }
}

static
void
TimeSendReceiveCallback(int8_t what) {
    switch (what) {
    case TIME_INT_START: {
            time_t now = time(NULL);
            struct tm brokenDown;
            localtime_r(&now, &brokenDown);

            char timestring[32];
            strftime(timestring, sizeof(timestring) - 1, "%F %T", &brokenDown);
            LOG("Systime: %s\n", timestring);

            s_In_SendReceive_Window = 1;
            TCP_Purge();
         } break;
    case TIME_INT_STOP:
        s_In_SendReceive_Window = 0;
        break;
    }
}

static
uint64_t GetTimestampInMillis() {
    uint64_t result = 0;
    timespec ts;
    if (0 == clock_gettime(CLOCK_MONOTONIC, &ts)) {
        result = ts.tv_sec * UINT64_C(1000);
        result += ts.tv_nsec / 1000000;
    }

    return result;
}

int
main(int argc, char** argv) {
    int listenSocketFD = -1;
    int epollFD = -1;
    sockaddr_un sa;
    bool semInitialzed = false;

    Time_Init();
    Time_Sync(0);
    Time_SetSyncWindowCallback(TimeSendReceiveCallback);
    Batman_Init();
    TCP_Init();
    TCP_SetDataReceivedCallback(TcpDataReceived);

    cmdlopt_set_app_name(RF24_NETWORK_APP_NAME);
<<<<<<< HEAD
    cmdlopt_set_app_version("1.3.0\nCopyright (c) 2016 Jean Gressmann <jean@0x42.de>");
=======
    cmdlopt_set_app_version("1.3.1\nCopyright (c) 2016, 2017 Jean Gressmann <jean@0x42.de>");
>>>>>>> r2
    cmdlopt_set_options(s_Options);
    int error = cmdlopt_parse_cmdl(argc, argv, NULL);

    switch (error) {
    case CMDLOPT_E_NONE:
        break;
    case CMDLOPT_E_HELP_REQUESTED:
    case CMDLOPT_E_VERSION_REQUESTED:
        error = 0;
        goto Exit;
    case CMDLOPT_E_UNKNOWN_OPTION:
        cmdlopt_fprint_help(stderr);
        goto Exit;
    case CMDLOPT_E_ERRNO:
        error = errno;
        goto Exit;
    case CMDLOPT_E_INVALID_PARAM:
        fprintf(stderr, "Internal program error %d\n", error);
        goto Exit;
    default:
        goto Exit;
    }

    Network_SetTtl(s_Network_Ttl);
    Network_SetAddress(s_Network_Address);
    Network_SetSendCallback(NetworkSendCallback);
    Time_SetStratum(s_Time_Stratum);

    if (!s_NetworkSocketPath || !*s_NetworkSocketPath) {
        fprintf(stderr, "Empty network UNIX socket path.\n");
        error = -1;
        goto Exit;
    }

    if (!s_PacketRouterSocketPath || !*s_PacketRouterSocketPath) {
        fprintf(stderr, "Empty packet router UNIX socket path.\n");
        error = -1;
        goto Exit;
    }

    listenSocketFD = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listenSocketFD == -1) {
        ERROR("Failed to create socket\n");
        error = errno;
        goto Exit;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, s_NetworkSocketPath, sizeof(sa.sun_path) - 1);
    unlink(s_NetworkSocketPath); // in case it exists
    if (bind(listenSocketFD, (sockaddr*)&sa, sizeof(sa)) < 0) {
        ERROR("Failed to bind AF_UNIX socket\n");
        error = errno;
        goto Exit;
    }

    if (listen(listenSocketFD, 1) < 0) {
        ERROR("Failed to listen on AF_UNIX socket\n");
        error = errno;
        goto Exit;
    }

    LOG("Time: %s\n", s_Time_Enabled ? "enabled" : "disabled");
    LOG("Batman: %s\n", s_Batman_Enabled ? "enabled" : "disabled");
    LOG("TCP: %s\n", s_Tcp_Enabled ? "enabled" : "disabled");
    LOG("Listening for connections on %s\n", s_NetworkSocketPath);

    s_PacketRouterSocketFD = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (s_PacketRouterSocketFD == -1) {
        ERROR("Failed to create socket\n");
        error = errno;
        goto Exit;
    }

    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, s_PacketRouterSocketPath, sizeof(sa.sun_path) - 1);
    if (connect(s_PacketRouterSocketFD, (sockaddr*)&sa, sizeof(sa)) < 0) {
        ERROR("Failed to connect to AF_UNIX socket %s\n", s_PacketRouterSocketPath);
        error = errno;
        goto Exit;
    }

    s_TimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (s_TimerFd < 0) {
        ERROR("Failed to create timer\n");
        error = errno;
        goto Exit;
    }

    epollFD = epoll_loop_create();
    if (epollFD == -1) {
        ERROR("Failed to create epoll instance\n");
        error = errno;
        goto Exit;
    }

    epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.data.fd = s_PacketRouterSocketFD;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0) {
        ERROR("Failed to add socket file descriptor to epoll\n");
        error = errno;
        goto Exit;
    }

    ev.data.fd = listenSocketFD;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0) {
        ERROR("Failed to add socket file descriptor to epoll\n");
        error = errno;
        goto Exit;
    }

    ev.data.fd = s_TimerFd;
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epollFD, EPOLL_CTL_ADD, ev.data.fd, &ev) < 0) {
        ERROR("Failed to add timer to epoll\n");
        error = errno;
        goto Exit;
    }

    error = sem_init(&s_Shutdown, 0, 0);
    if (error < 0) {
        ERROR("Failed to create sem\n");
        error = errno;
        goto Exit;
    }
    semInitialzed = true;

    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGPIPE, SIG_IGN);

    epoll_callback_data ecd;
    memset(&ecd, 0, sizeof(ecd));

    ecd.callback = EPollPacketRouterHandler;
    epoll_loop_set_callback(s_PacketRouterSocketFD, ecd);

    ecd.callback = EPollAcceptHandler;
    epoll_loop_set_callback(listenSocketFD, ecd);

    ecd.callback = EPollTimerHandler;
    epoll_loop_set_callback(s_TimerFd, ecd);

    s_LastIterationsTimestamp = GetTimestampInMillis();

    itimerspec spec;
    spec.it_interval.tv_sec = 0;
    spec.it_interval.tv_nsec = 0;
    spec.it_value.tv_sec = 0;
    spec.it_value.tv_nsec = 1;
    if (timerfd_settime(s_TimerFd, 0, &spec, NULL) < 0) {
        ERROR("Failed to set timer\n");
        error = errno;
        goto Exit;
    }

    while (sem_wait(&s_Shutdown) == -1  && errno == EINTR);

Exit:
    if (s_TimerFd >= 0) {
        memset(&ecd, 0, sizeof(ecd));
        epoll_loop_set_callback(s_TimerFd, ecd);
        safe_close_ref(&s_TimerFd);
    }

    if (s_PacketRouterSocketFD >= 0) {
        memset(&ecd, 0, sizeof(ecd));
        epoll_loop_set_callback(s_PacketRouterSocketFD, ecd);
        safe_close(s_PacketRouterSocketFD);
    }

    if (listenSocketFD >= 0) {
        memset(&ecd, 0, sizeof(ecd));
        epoll_loop_set_callback(listenSocketFD, ecd);
        shutdown(listenSocketFD, SHUT_RDWR);
        safe_close(listenSocketFD);
    }

    if (s_NetworkSocketPath) {
        unlink(s_NetworkSocketPath);
    }

    epoll_loop_destroy();

    for (HandleSet::const_iterator it = s_Connections.begin(), end = s_Connections.end();
         it != end; ++it) {
        DEBUG("Shutdown client %d\n", *it);
        shutdown(*it, SHUT_RDWR);
        safe_close(*it);
    }

    if (semInitialzed) {
        sem_destroy(&s_Shutdown);
    }

    TCP_Uninit();
    Batman_Uninit();
    Time_Uninit();

    if (error > 0) {
        fprintf(stderr, "%s (%d)\n", strerror(error), error);
    }
    return error;
}


static
void
EPollPacketRouterHandler(void *ctx, epoll_event *ev) {
    assert(ev);
    if (ev->events & (EPOLLHUP | EPOLLERR)) {
        ERROR("Connection lost!\n");
        Shutdown();
    } else {
        if (ev->events & EPOLLIN) {
            NetworkPacket packet;
            for (ssize_t r = 1; r; ) {
                r = read(ev->data.fd, &packet, sizeof(packet));
                if (r < 0) {
                    switch (errno) {
                    case EAGAIN:
                    case EINTR:
                    case EBADF:
                        r = 0;
                        break;
                    default:
                        r = 0;
                        ERROR("Unhandled error %s (%d) while reading from connection %d\n", strerror(errno), errno, ev->data.fd);
                        Shutdown();
                        break;
                    }
                } else if (r == sizeof(packet)) {
<<<<<<< HEAD
                    if (!s_Time_Enabled || s_In_SendReceive_Window) {
=======
                    if (s_In_SendReceive_Window) {
>>>>>>> r2
                        switch (packet.Type) {
                        case BATMAN_PACKET_TYPE:
                            if (s_Batman_Enabled) {
                                Batman_Process(&packet);
                            }
                            break;
                        case TIME_PACKET_TYPE:
                            if (s_Time_Enabled) {
                                Time_Process(&packet);
                            }
                            break;
                        case TCP_PACKET_TYPE:
                            if (s_Tcp_Enabled) {
                                TCP_Process(&packet);
                            }
                            break;
                        }
                    }
                } else {
                    // all clients must write payload size chunks
                    ERROR("Read %d instead of %d bytes, shutting down %d\n", (int)r, (int)(sizeof(packet)), ev->data.fd);
                    Shutdown();
                    break;
                }
            }
        }
    }
}


static
void
EPollTimerHandler(void *ctx, epoll_event *ev) {
    assert(ev);

    if (ev->events & (EPOLLHUP | EPOLLERR)) {
        epoll_callback_data ecd;
        memset(&ecd, 0, sizeof(ecd));
        epoll_loop_set_callback(ev->data.fd, ecd);
        ERROR("Timer lost!\n");
        Shutdown();
    } else if (ev->events & EPOLLIN) {
        while (true) {
            uint64_t c;
            ssize_t r = read(ev->data.fd, &c, sizeof(c));
            if (r < 0) {
                switch (errno) {
                case EAGAIN:
                case EINTR:
                    r = 0;
                    break;
                default:
                    ERROR("Could not read from timer fd (%d, %s)\n", errno, strerror(errno));
                    Shutdown();
                    return;
                }
            }

            if (r == 0) {
                break;
            }
        }

        const uint64_t now = GetTimestampInMillis();
        const uint64_t millisElapsed = (now - s_LastIterationsTimestamp);
        s_LastIterationsTimestamp = now;

        bool arm = false;

        if (s_Time_Enabled) {
            if (millisElapsed) {
                Time_Update(millisElapsed);
<<<<<<< HEAD
=======
                if (s_Time_BroadcastOnTick) {
                    const uint64_t millisElapsed = now - s_LastTimeBroadcastTimestamp;
                    if (millisElapsed >= s_Time_Tick_Millis) {
                        s_LastTimeBroadcastTimestamp = now;
                        if (s_Time_PrintTti) {
                            LOG("Tti: %u\n", Time_TimeToNextInterval());
                        }
                        Time_BroadcastTime();
                    }
                }
>>>>>>> r2
            }

            arm = true;
        }

        const uint32_t left = Time_TimeToNextInterval();
        uint32_t millis = left < s_Time_Tick_Millis ? left : s_Time_Tick_Millis;

<<<<<<< HEAD
        if (s_Batman_Enabled &&
            (!s_Time_Enabled || s_In_SendReceive_Window)) {
=======
        if (s_Batman_Enabled && s_In_SendReceive_Window) {
>>>>>>> r2
            Batman_Update();
            Batman_Broadcast();
            arm = true;
        }

<<<<<<< HEAD
        if (s_Tcp_Enabled &&
            (!s_Time_Enabled || s_In_SendReceive_Window)) {
=======
        if (s_Tcp_Enabled && s_In_SendReceive_Window) {
>>>>>>> r2
            TCP_Update();
            if (millis > 8) {
                millis = 8;
            }

            arm = true;
        }

        if (arm) {
            // re-arm timer
            itimerspec spec;
            spec.it_interval.tv_sec = 0;
            spec.it_interval.tv_nsec = 0;
            uint64_t nanoSeconds = millis ? millis * UINT64_C(1000000) : 1;
            spec.it_value.tv_sec = nanoSeconds / UINT64_C(1000000000);
            spec.it_value.tv_nsec = static_cast<long>(nanoSeconds - static_cast<uint64_t>(spec.it_value.tv_sec) * UINT64_C(1000000000));
            if (timerfd_settime(s_TimerFd, 0, &spec, NULL) < 0) {
                ERROR("Could not re-arm timer (%d, %s)\n", errno, strerror(errno));
                Shutdown();
            }
        }
    }
}


static
void
EPollAcceptHandler(void *ctx, epoll_event *ev) {
    assert(ev);

    if (ev->events & (EPOLLHUP | EPOLLERR)) {
        epoll_callback_data ecd;
        memset(&ecd, 0, sizeof(ecd));
        epoll_loop_set_callback(ev->data.fd, ecd);

        ERROR("Listen socket gone!\n");
        Shutdown();
    } else if (ev->events & EPOLLIN){
        sockaddr_un remote;
        socklen_t length = sizeof(remote);
        int fd = accept4(ev->data.fd, (sockaddr*)&remote, &length, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd >= 0) {
            ev->data.fd = fd;
            ev->events = EPOLLIN | EPOLLET;
            if (epoll_ctl(epoll_loop_get_fd(), EPOLL_CTL_ADD, ev->data.fd, ev) == 0) {
                epoll_callback_data ecd;
                memset(&ecd, 0, sizeof(ecd));
                ecd.callback = EPollConnectionDataHandler;
                epoll_loop_set_callback(fd, ecd);
                s_Connections.insert(fd);
                DEBUG("Add client %d\n", fd);
            } else {
                safe_close(fd);
            }
        }
    }
}

static
inline
int
GetHexDigit(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }

    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }

    return -1;
}

static
bool
GetAddress(char* input, uint8_t& address) {
    assert(input);
    int high = GetHexDigit(input[0]);
    int low = GetHexDigit(input[1]);
    address = (uint8_t)((high << 4)  | low);
    return high != -1 && low != -1;
}

static
void
EPollConnectionDataHandler(void *ctx, epoll_event *ev) {
    assert(ev);
    if (ev->events & (EPOLLHUP | EPOLLERR)) {
        epoll_callback_data ecd;
        memset(&ecd, 0, sizeof(ecd));
        epoll_loop_set_callback(ev->data.fd, ecd);
        s_TcpConnections.erase(ev->data.fd);
        s_Connections.erase(ev->data.fd);
        safe_close(ev->data.fd);
        DEBUG("Remove client %d\n", ev->data.fd);
    } else {
        if (ev->events & EPOLLIN) {
            char payload[3];
            for (ssize_t r = 1; r; ) {
                r = read(ev->data.fd, payload, sizeof(payload));

                if (r < 0) {
                    switch (errno) {
                    case EAGAIN:
                    case EINTR:
                    case EBADF:
                        r = 0;
                        break;
                    default:
                        r = 0;
                        shutdown(ev->data.fd, SHUT_RDWR);
                        break;
                    }
                } else {
                    switch (payload[0]) {
                    case 'T': {
                            char buffer[32];
                            int chars = snprintf(buffer, sizeof(buffer), "%u", Time_Now());
                            write(ev->data.fd, buffer, chars);
                        } break;
                    case 'N': { // network id query
                            if (r != 3) {
                                shutdown(ev->data.fd, SHUT_RDWR);
                            } else {
                                uint8_t address = 0xff;
                                if (r) {
                                    if (!GetAddress(payload + 1, address)) {
                                        address = 0xff;
                                    }
                                }

                                if (s_Batman_Enabled) {
                                    address = Batman_Route(address);
                                }

                                write(ev->data.fd, "0123456789abcdef" + ((address >> 4) & 15), 1);
                                write(ev->data.fd, "0123456789abcdef" + (address & 15), 1);
                            }
                        } break;
                    case '+': // add TCP client
                        if (s_Tcp_Enabled) {
                            s_TcpConnections.insert(ev->data.fd);
                        }
                        break;
                    case '-': // remove TCP client
                        if (s_Tcp_Enabled) {
                            s_TcpConnections.erase(ev->data.fd);
                        }
                        break;
                    }
                }
            }
        }
    }
}

