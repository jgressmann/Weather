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
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <toe/cmdlopt.h>
#include <linuxapi/linuxapi.h>

#include "Globals.h"

#define APPNAME "rf24-ping"

#define ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)
#define LOG(...) fprintf(stdout, __VA_ARGS__)

#if 0
#   define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#   define DEBUG(...)
#endif

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

static const char* s_RF24PacketRouterSocketPath = RF24_PACKET_ROUTER_SOCKET_PATH;
static
int
RF24PacketRouterSocketPath_Parser(void*, char* arg) {
    s_RF24PacketRouterSocketPath = arg;
    return 0;
}


static unsigned s_MillisecondsToSleep = 1000;
static
int
Sleep_Parser(void*, char* arg) {
    return UnsignedParser(arg, s_MillisecondsToSleep);
}

static const cmdlopt_opt s_Options[] = {
    { "rf24-packet-router-socket-path", "Path to UNIX socket. Defaults to " RF24_PACKET_ROUTER_SOCKET_PATH, 0, 0x100, s_Dummy_Arg, RF24PacketRouterSocketPath_Parser},
    { "sleep", "Milliseconds to sleep between pings. Defaults to 1000", 's', 0x101, s_Dummy_Arg, Sleep_Parser},
    CMDLOPT_COMMON_OPTIONS,
    CMDLOPT_OPTION_TERMINATOR
};

int
main(int argc, char** argv) {
    int socketFd = -1;
    sockaddr_un sa;
    char buffer[32];
    uint32_t counter = 0;

    cmdlopt_set_app_name(APPNAME);
    cmdlopt_set_app_version("1.0\nCopyright (c) 2016 Jean Gressmann <jean@0x42.de>");
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

    if (!s_RF24PacketRouterSocketPath || !*s_RF24PacketRouterSocketPath) {
        fprintf(stderr, "Empty Rf24 packet router UNIX socket path.\n");
        error = -1;
        goto Exit;
    }

    memset(&sa, 0, sizeof(sa));


    socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd == -1) {
        ERROR("Failed to create socket\n");
        error = errno;
        goto Exit;
    }

    sa.sun_family = AF_UNIX;
    strncpy(sa.sun_path, s_RF24PacketRouterSocketPath, sizeof(sa.sun_path) - 1);
    if (connect(socketFd, (sockaddr*)&sa, sizeof(sa)) < 0) {
        ERROR("Failed to connect to AF_UNIX socket %s\n", s_RF24PacketRouterSocketPath);
        error = errno;
        goto Exit;
    }

    error = 0;


    while (1) {
        snprintf(buffer, sizeof(buffer), "ping %u\n", counter++);
        LOG(buffer);
        write(socketFd, buffer, sizeof(buffer));
        usleep(s_MillisecondsToSleep * 1000);
    }

Exit:
    if (socketFd >= 0) {
        safe_close(socketFd);
    }

    if (error > 0) {
        fprintf(stderr, "%s (%d)\n", strerror(error), error);
    }
    return error;
}


