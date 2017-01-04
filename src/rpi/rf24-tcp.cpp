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
#include <toe/buffer.h>
#include <linuxapi/linuxapi.h>


#include "../../TCP.h"

#include "Globals.h"


#define APPNAME "rf24-tcp"


#define ERROR(...) fprintf(stderr, "ERROR: " __VA_ARGS__)
#define LOG(...) fprintf(stdout, __VA_ARGS__)

#if 0
#   define DEBUG(...) fprintf(stderr, __VA_ARGS__)
#else
#   define DEBUG(...)
#endif

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


static const cmdlopt_opt s_Options[] = {
    { "network-socket-path", "Path to UNIX socket. Defaults to " RF24_NETWORK_SOCKET_PATH, 0, 0x100, s_Dummy_Arg, NetworkSocketPath_Parser },
    CMDLOPT_COMMON_OPTIONS,
    CMDLOPT_OPTION_TERMINATOR
};



static
void
PrintTime(FILE* f) {
    time_t now = time(NULL);
    struct tm brokenDown;
    localtime_r(&now, &brokenDown);

    char timestring[32];
    strftime(timestring, sizeof(timestring) - 1, "%F %T", &brokenDown);
    fprintf(f, "%s", timestring);
}

int
main(int argc, char** argv) {
    int socketFd = -1;
    sockaddr_un sa;

    cmdlopt_set_app_name(APPNAME);
    cmdlopt_set_app_version("1.1.0\nCopyright (c) 2016 Jean Gressmann <jean@0x42.de>");
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

    if (!s_NetworkSocketPath || !*s_NetworkSocketPath) {
        fprintf(stderr, "Empty network socket UNIX socket path.\n");
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
    strncpy(sa.sun_path, s_NetworkSocketPath, sizeof(sa.sun_path) - 1);
    if (connect(socketFd, (sockaddr*)&sa, sizeof(sa)) < 0) {
        ERROR("Failed to connect to AF_UNIX socket %s\n", s_NetworkSocketPath);
        error = errno;
        goto Exit;
    }

    error = write(socketFd, "+", 1);
    if (error != 1) {
        ERROR("Failed to register as TCP listener\n");
        error = errno;
        goto Exit;
    }

    error = 0;
    signal(SIGPIPE, SIG_IGN); // for the stupid socket

    ssize_t ro;
    uint8_t header[2];
    char tcpPayload[TCP_PAYLOAD_SIZE+1];
    ro = 0;
    while (1) {
        ssize_t r = read(socketFd, header + ro, sizeof(header) - ro);
        if (r == -1) {
            switch (r) {
            case EINTR:
                break;
            default:
                goto Exit;
            }
        } if (r == 0) { // connection closed
            goto Exit;
        } else {
            ro += r;
            if (ro == sizeof(header)){
                ro = 0;
                uint8_t sender = header[0];
                (void)sender;
                uint8_t size = header[1];
                DEBUG("Packet from %02x, size %u\n", sender, size);
                while (1) {
                    r = read(socketFd, tcpPayload + ro, size - ro);
                    if (r == -1) {
                        switch (r) {
                        case EINTR:
                            break;
                        default:
                            goto Exit;
                        }
                    } if (r == 0) {  // connection closed
                        goto Exit;
                    } else {
                        ro += r;
                        if (ro == size) {
                            ro = 0;
                            tcpPayload[size] = 0;
                            fprintf(stdout, "%s\n", tcpPayload);
                            break;
                        }
                    }
                }
            }
        }
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


