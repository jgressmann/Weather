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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
//#include <pwd.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include <errno.h>

#include <algorithm>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <toe/cmdlopt.h>
#include <toe/buffer.h>
#include <linuxapi/linuxapi.h>
#include <RF24/RF24.h>

#include "Globals.h"


#define NUMBER_OF_READ_PIPES 6
#define MAX_PAYLOAD_SIZE 32

#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)
#define ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)
#define LOG(...) fprintf(stdout, __VA_ARGS__)

#if 1
#   define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#   define DEBUG(...)
#endif


static buffer* s_Connections;
#define int_begin(buf) ((int*)buf->beg)
#define int_end(buf) ((int*)buf->end)
#define int_reserve(buf, count) buf_resize(buf, count*sizeof(int))
#define int_pop_back(buf) buf->end -= sizeof(int)
#define int_used(buf) (buf_used(buf)/sizeof(int))



static void EPollAcceptHandler(void* ctx, epoll_event* ev);
static void EPollConnectionDataHandler(void* ctx, epoll_event* ev);
static void EPollTimerHandler(void *ctx, epoll_event *ev);

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

static
inline
int ByteParser(const char* arg, uint8_t& value) {
    return UnsignedParser(arg, value);
}

static uint8_t s_Channel = 76;
static
int
Channel_Parser(void*, char* arg) {
    return ByteParser(arg, s_Channel);
}


static bool s_Dump = false;
static
int
Dump_Parser(void*, char* arg) {
    s_Dump = true;
    return 0;
}

static uint8_t s_Read_Pipes[NUMBER_OF_READ_PIPES] = { 1, 2, 3, 4, 5, 6 };
static
int
Read_Pipe_Parser(void*, char* arg) {
    char* result;
    char* end;
    int index = 0;
    while ((result = strtok_r(arg, ";, \t", &end)) != NULL && index < NUMBER_OF_READ_PIPES) {
        arg = NULL;
        int error = ByteParser(result, s_Read_Pipes[index]);
        if (error) {
            ERROR("Pipe index %d\n", index);
            return error;
        }
        ++index;
    }
    return 0;
}

static uint8_t s_WriteId = 1;
static
int
Write_Pipe_Parser(void*, char* arg) {
    return ByteParser(arg, s_WriteId);
}


static const cmdlopt_arg s_Dummy_Arg[] = {
    CMDLOPT_ARGUMENT_TERMINATOR
};

static const cmdlopt_arg s_BaseAddress_Arg[] = {
    { "hex string", "the most signficant 4 bytes of the pipe address", "0xdeadbeef" },
    CMDLOPT_ARGUMENT_TERMINATOR
};
static uint32_t s_BaseAddress = 0xdeadbeef;
static
int
Base_Address_Parser(void*, char* arg) {
    int error = 0;
    if (arg && *arg) {
        // strip off 0(x|X) prefix if present
        if (arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X')) {
            arg += 2;
        }
        char* end = NULL;
        s_BaseAddress = strtoul(arg, &end, 16);
        if (!end || end == arg) {
            error = -1;
        }
    } else {
        error = -1;
    }

    if (error) {
        ERROR("Argument %s is not a valid hex value\n", arg);
    }

    return error;
}

static const cmdlopt_arg s_Data_Rate_Arg[] = {
    { "0", "250KBPS (plus modesls only)", NULL },
    { "1", "1MBPS", NULL },
    { "2", "2MBPS (default)", NULL },
    CMDLOPT_ARGUMENT_TERMINATOR
};

static uint8_t s_DataRate = 2;
static const rf24_datarate_e s_DataRateTable[] = { RF24_250KBPS, RF24_1MBPS, RF24_2MBPS };
static
int
DataRate_Parser(void*, char* arg) {
    int error = ByteParser(arg, s_DataRate);\
    if (!error) {
        switch (s_DataRate) {
        case 0: case 1: case 2:
            break;
        default:
            ERROR("Invalid data rate %d\n", s_DataRate);
            error = -1;
        }
    }
    return error;
}

static const cmdlopt_arg s_Power_Level_Arg[] = {
    { "0", "min", NULL },
    { "1", "low", NULL },
    { "2", "high", NULL },
    { "3", "max (default)", NULL },
    CMDLOPT_ARGUMENT_TERMINATOR
};
static uint8_t s_PowerLevel = 3;
static
int
Power_Level_Parser(void*, char* arg) {
    int error = ByteParser(arg, s_PowerLevel);\
    if (!error) {
        switch (s_PowerLevel) {
        case RF24_PA_MIN:
        case RF24_PA_LOW:
        case RF24_PA_HIGH:
        case RF24_PA_MAX:
            break;
        default:
            ERROR("Invalid power level %d\n", s_PowerLevel);
            error = -1;
        }
    }
    return error;
}

static const cmdlopt_arg s_Crc_Length_Arg[] = {
    { "0", "disabled", NULL },
    { "1", "8 bits", NULL },
    { "2", "16 bits (default)", NULL },
    CMDLOPT_ARGUMENT_TERMINATOR
};
static uint8_t s_Crc_Length = 2;
static
int
Crc_Length_Parser(void*, char* arg) {
    int error = ByteParser(arg, s_Crc_Length);\
    if (!error) {
        switch (s_Crc_Length) {
        case RF24_CRC_DISABLED:
        case RF24_CRC_8:
        case RF24_CRC_16:
            break;
        default:
            ERROR("Invalid CRC length %d\n", s_Crc_Length);
            error = -1;
        }
    }
    return error;
}

static uint8_t s_PayloadSize = MAX_PAYLOAD_SIZE;
static
int
Payload_Parser(void*, char* arg) {
    int error = ByteParser(arg, s_PayloadSize);\
    if (!error) {
        if (!s_PayloadSize || s_PayloadSize > MAX_PAYLOAD_SIZE) {
            ERROR("Invalid payload size %d. Max payload is " STRINGIFY(MAX_PAYLOAD_SIZE) " bytes.\n", s_PayloadSize);
            error = -1;
        }
    }
    return error;
}

static uint32_t s_Sleep = 10000; // 10ms
static
int
Sleep_Parser(void*, char* arg) {
    return UnsignedParser(arg, s_Sleep);
}

static const char* s_SocketPath = RF24_PACKET_ROUTER_SOCKET_PATH;
static
int
SocketPath_Parser(void*, char* arg) {
    s_SocketPath = arg;
    return 0;
}



static const cmdlopt_opt s_Options[] = {
    { "rf24-channel", "channel to use. Defaults to 76.", 'c', 0x100, s_Dummy_Arg, Channel_Parser },
    { "rf24-base-address", "<value>", 'b', 0x101, s_BaseAddress_Arg, Base_Address_Parser },
    { "rf24-read-pipes", "Read pipe ids. Defaults to 1, 2, 3, 4, 5, 6.", 'r', 0x102, s_Dummy_Arg, Read_Pipe_Parser },
    { "rf24-write-pipe", "Write pipe id. Defaults to 1.", 'w', 0x103, s_Dummy_Arg, Write_Pipe_Parser },
    { "rf24-data-rate", "<value>", 'd', 0x104, s_Data_Rate_Arg, DataRate_Parser },
    { "rf24-payload-size", "Size of payload. Defaults to " STRINGIFY(MAX_PAYLOAD_SIZE) ".", 0, 0x105, s_Dummy_Arg, Payload_Parser },
    { "rf24-power-level", "<value>", 'p', 0x106, s_Power_Level_Arg, Power_Level_Parser },
    { "rf24-crc-length", "<value>", 0, 0x107, s_Crc_Length_Arg, Crc_Length_Parser },
    { "rf24-show", "Prints radio setup", 0, 0x110, NULL, Dump_Parser },
    { "sleep", "Microseconds to sleep between polls. Defauls to 10000.", 0, 0x111, s_Dummy_Arg, Sleep_Parser },
    //{ "io-mode", "Select I/O mode. Defaults to line", 0, 0x112, s_Io_Mode_Arg, Io_Mode_Parser },
    { "socket-path", "Path to UNIX socket. Defaults to " RF24_PACKET_ROUTER_SOCKET_PATH, 's', 0x113, s_Dummy_Arg, SocketPath_Parser },
    CMDLOPT_COMMON_OPTIONS,
    CMDLOPT_OPTION_TERMINATOR
};

static RF24 s_Radio(RPI_GPIO_P1_26, RPI_GPIO_P1_24);

static
void
SetupRadio() {
    // Setup the radio
    s_Radio.begin();
    s_Radio.setPALevel(s_PowerLevel);
    s_Radio.setAddressWidth(5); // full 5 byte addresses
    s_Radio.maskIRQ(true, true, true); // interrupt line not connected
    s_Radio.setChannel(s_Channel);
    s_Radio.setPayloadSize(s_PayloadSize);
    s_Radio.setAutoAck(false);
    s_Radio.setRetries(15, 0);
    s_Radio.setDataRate(s_DataRateTable[s_DataRate]);
    s_Radio.setCRCLength((rf24_crclength_e)s_Crc_Length);


    // set read pipe addresses
    uint8_t addr[5];
    addr[1] = s_BaseAddress & 0xff;
    addr[2] = (s_BaseAddress >> 8) & 0xff;
    addr[3] = (s_BaseAddress >> 16) & 0xff;
    addr[4] = (s_BaseAddress >> 24) & 0xff;

    // read
    for (int i = 0; i < NUMBER_OF_READ_PIPES; ++i) {
        addr[0] = s_Read_Pipes[i];
        s_Radio.openReadingPipe(i, addr);
    }

    // write
    addr[0] = s_WriteId;
    s_Radio.openWritingPipe(addr);
    s_Radio.startListening();
}

static int s_TimerFd = -1;


int
main(int argc, char** argv) {
    int mySocketFD = -1;
    int epollFD = -1;
    sockaddr_un sa;
    bool semInitialzed = false;

    cmdlopt_set_app_name(RF24_PACKET_ROUTER_APP_NAME);
    cmdlopt_set_app_version("1.1\nCopyright (c) 2016 Jean Gressmann <jean@0x42.de>");
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

    if (!s_SocketPath || !*s_SocketPath) {
        fprintf(stderr, "Empty UNIX socket path.\n");
        error = -1;
        goto Exit;
    }

    if (0 != getuid()) {
        fprintf(stderr, "This program must be run as root.\n");
        error = -1;
        goto Exit;
    }

    SetupRadio();

    if (s_Dump) {
        s_Radio.printDetails();
        goto Exit;
    }

    s_Connections = buf_alloc(64);
    if (!s_Connections) {
        errno = ENOMEM;
        goto Exit;
    }

    mySocketFD = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (mySocketFD == -1) {
        ERROR("Failed to create socket\n");
        error = errno;
        goto Exit;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, s_SocketPath, sizeof(sa.sun_path) - 1);
    unlink(s_SocketPath); // in case it exists
    if (bind(mySocketFD, (sockaddr*)&sa, sizeof(sa)) < 0) {
        ERROR("Failed to bind AF_UNIX socket\n");
        error = errno;
        goto Exit;
    }

    if (listen(mySocketFD, 1) < 0) {
        ERROR("Failed to listen on AF_UNIX socket\n");
        error = errno;
        goto Exit;
    }

    LOG("Listening for connections on %s\n", s_SocketPath);

    if (-1 == chmod(s_SocketPath, 0666)) {
        ERROR("Could not change permissions of socket %s to 666\n", s_SocketPath);
        error = errno;
        goto Exit;
    }

//    if (-1 == chown(s_SocketPath, u, g))
//    {
//        ERROR("Could not change ownership of socket %s to uid %d, gid %d\n", s_SocketPath, u, g);
//        error = errno;
//        goto Exit;
//    }

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
    ev.data.fd = mySocketFD;
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

    ecd.callback = EPollAcceptHandler;
    epoll_loop_set_callback(mySocketFD, ecd);

    ecd.callback = EPollTimerHandler;
    epoll_loop_set_callback(s_TimerFd, ecd);

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

    memset(&ecd, 0, sizeof(ecd));
    if (mySocketFD >= 0) {
        epoll_loop_set_callback(mySocketFD, ecd);
        safe_close(mySocketFD);
        unlink(s_SocketPath);
    }

    // close all connections
    if (s_Connections) {
        for (int* it = int_begin(s_Connections); it != int_end(s_Connections); ++it) {
            safe_close(*it++);
        }
        buf_free(s_Connections);
    }

    epoll_loop_destroy();

    if (semInitialzed) sem_destroy(&s_Shutdown);

    if (error > 0) {
        fprintf(stderr, "%s (%d)\n", strerror(error), error);
    }
    return error;
}

static
void
EPollAcceptHandler(void *ctx, epoll_event *ev) {
    assert(ev);

    if (ev->events & (EPOLLHUP | EPOLLERR)) {
        epoll_callback_data ecd;
        memset(&ecd, 0, sizeof(ecd));
        epoll_loop_set_callback(ev->data.fd, ecd);

        ERROR("Socket closed!\n");
        Shutdown();
    } else if (ev->events & EPOLLIN){
        sockaddr_un remote;
        socklen_t length = sizeof(remote);
        int fd = accept4(ev->data.fd, (sockaddr*)&remote, &length, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd >= 0) {
            ev->data.fd = fd;
            ev->events = EPOLLIN | EPOLLET;
            if (epoll_ctl(epoll_loop_get_fd(), EPOLL_CTL_ADD, ev->data.fd, ev) == 0) {
                if (!int_reserve(s_Connections, 1)) {
                    ERROR("Out of memory!\n");
                    Shutdown();
                } else {
                    if (!int_reserve(s_Connections, 1)) {
                        ERROR("Out of memory!\n");
                        Shutdown();
                    } else {
                        *int_end(s_Connections) = fd;
                        s_Connections->end += sizeof(fd);
                    }

                    epoll_callback_data ecd;
                    memset(&ecd, 0, sizeof(ecd));
                    ecd.callback = EPollConnectionDataHandler;
                    epoll_loop_set_callback(fd, ecd);
                }
            } else {
                safe_close(fd);
            }
        }
    }
}

static
void
EPollConnectionDataHandler(void *ctx, epoll_event *ev) {
    assert(ev);
    if (ev->events & (EPOLLHUP | EPOLLERR)) {
        epoll_callback_data ecd;
        memset(&ecd, 0, sizeof(ecd));
        epoll_loop_set_callback(ev->data.fd, ecd);

        safe_close(ev->data.fd);

        // remove fd from connection list
        int* end = int_end(s_Connections);
        int* it = std::find(int_begin(s_Connections), end, ev->data.fd);
        if (it != end) {
            --end;
            *it = *end;
            int_pop_back(s_Connections);
        }
    } else {
        if (ev->events & EPOLLIN) {
            char payload[MAX_PAYLOAD_SIZE];
            for (ssize_t r = 1; r; ) {
                r = read(ev->data.fd, payload, s_PayloadSize);

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
                } else if (r == s_PayloadSize) {
                    //DEBUG("Radio send\n");
                    s_Radio.stopListening();
                    if (!s_Radio.write(payload, s_PayloadSize)) {
                        ERROR("Failed to send telegram\n");
                        if (s_Radio.failureDetected) {
                            s_Radio.failureDetected = 0;
                            SetupRadio();
                        }
                    }
                    s_Radio.startListening();
                } else {
                    // all clients must write payload size chunks
                    ERROR("Read %d instead of %d bytes, shutting down %d\n", (int)r, (int)s_PayloadSize, ev->data.fd);
                    r = 0;
                    shutdown(ev->data.fd, SHUT_RDWR);
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

        //DEBUG("Timer expired\n");

        char buffer[MAX_PAYLOAD_SIZE];
        while (s_Radio.available()) {
            //DEBUG("Radio available\n");
            //DEBUG("%u connections\n", (unsigned)(int_used(s_Connections)));
            s_Radio.read(buffer, s_PayloadSize);
            for (int* it = int_begin(s_Connections), * end = int_end(s_Connections);
                it != end; ++it) {
                ssize_t w = write(*it, buffer, s_PayloadSize);
                if (w < 0) {
                    switch (errno) {
                    case EINTR:
                        --it; // re-try connection
                        break;
                    default:
                        // ignore all other errors, will get HUP
                        // in case connection is gone
                        break;
                    }
                } else {
                    //DEBUG("Forward to %d\n", *it);
                }
            }
        }

        // re-arm timer
        itimerspec spec;
        spec.it_interval.tv_sec = 0;
        spec.it_interval.tv_nsec = 0;
        uint64_t nanoSeconds = static_cast<uint64_t>(s_Sleep) * 1000;
        spec.it_value.tv_sec = nanoSeconds / 1000000000;
        spec.it_value.tv_nsec = static_cast<long>(nanoSeconds - static_cast<uint64_t>(spec.it_value.tv_sec) * 1000000000);
        if (timerfd_settime(s_TimerFd, 0, &spec, NULL) < 0) {
            ERROR("Could not re-arm timer (%d, %s)\n", errno, strerror(errno));
            Shutdown();
        }
    }
}
