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

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <toe/cmdlopt.h>
#include <linuxapi/linuxapi.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <inttypes.h>
#include <readline/history.h>
#include <readline/readline.h>

#include <vector>

#include "../Globals.h"

#define APPNAME "rf24-cli"
#define FAIL(...) fprintf(stderr, "ERROR: " __VA_ARGS__)

static int ProcessInput(char* str);

static
void FormatCommands();

struct Arg {
    const char* Name;
    const char* Description;
    const char* Example;
};

struct Command {
    const char* Name;
    const char* Description;
    const Arg* Args;
};

static const Command Commands[] = {
    { "help", "Prints this help", NULL },
    { "route", "<2 hex chars> gets the netork id of next hop in route", NULL },
    { "quit", "Exits the program", NULL },
    { NULL, NULL, NULL },
};


static const cmdlopt_opt s_Options[] = {
    CMDLOPT_COMMON_OPTIONS,
    CMDLOPT_OPTION_TERMINATOR
};

const int QuitError = 1;
static int s_Socket = -1;
static char s_CommandsHelp[512];
static std::vector<char> s_Buffer;

int
main(int argc, char** argv) {
    std::vector<char> command;
    sockaddr_un socketAddress;

    FormatCommands();

    cmdlopt_set_help_header("Usage: " APPNAME " [cmd ...]");
    cmdlopt_set_help_footer(s_CommandsHelp);
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
        FAIL("Internal program error %d\n", error);
        goto Exit;
    default:
        goto Exit;
    }

    for (int i = optind; i < argc; ++i) {
        const char* p = argv[i];

        while (*p) {
            command.push_back(*p++);
        }

        command.push_back(' ');
    }

    if (command.size()) {
        command.push_back(0);
    }

    s_Socket = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s_Socket == -1) {
        FAIL("Failed to create socket\n");
        error = errno;
        goto Exit;
    }

    memset(&socketAddress, 0, sizeof(socketAddress));
    socketAddress.sun_family = AF_UNIX;
    strncpy(socketAddress.sun_path, RF24_NETWORK_SOCKET_PATH, sizeof(socketAddress.sun_path));
    if (connect(s_Socket, (sockaddr*)&socketAddress, sizeof(socketAddress)) < 0) {
        FAIL("Failed to connect to AF_UNIX socket %s\n", socketAddress.sun_path);
        error = errno;
        goto Exit;
    }

    if (command.size()) {
        error = ProcessInput(&command[0]);
    } else {
        for (bool done = false; !done; ) {
            char* input = readline("rf24>");

            if (!input) {
                break;
            }

            if (!*input) {
                continue;
            }

            add_history(input);

            error = ProcessInput(input);

            switch (error) {
            case 0:
                break;
            case QuitError:
                error = 0;
            default:
                done = true;
                break;
            }

            if (s_Socket == -1) {
                done = true;
            }

            free(input);
        }
    }

Exit:
    if (s_Socket >= 0) safe_close(s_Socket);
    if (error > 0) {
        fprintf(stderr, "%s (%d)\n", strerror(error), error);
    }
    return error;
}


static
void
FormatCommands() {
    size_t offset = 0;
    offset += snprintf(s_CommandsHelp + offset, sizeof(s_CommandsHelp) - offset, "Commands:\n");
    for (size_t i = 0; Commands[i].Name; ++i) {
        offset += snprintf(s_CommandsHelp + offset, sizeof(s_CommandsHelp) - offset, "%8s: %s\n", Commands[i].Name, Commands[i].Description);
        if (Commands[i].Args) {
            const Arg* args = Commands[i].Args;

            for (size_t j = 0; args[j].Name; ++j) {
                offset += snprintf(s_CommandsHelp + offset, sizeof(s_CommandsHelp) - offset, "%-10s%s: %s\n", "", args[j].Name, args[j].Description);
                if (args[j].Example) {
                    offset += snprintf(s_CommandsHelp + offset, sizeof(s_CommandsHelp) - offset, "%-10sExample: %s\n", "", args[j].Example);
                }
            }
        }
    }
}



static
size_t
Sanatize(char*& input) {
    while (whitespace(*input)) {
        ++input;
    }

    size_t len = strlen(input);

    while (len && whitespace(input[len - 1])) {
        input[--len] = 0;
    }

    return len;
}

static
bool
Write(const char* ptr, size_t bytes) {
Start:
    ssize_t w = write(s_Socket, ptr, bytes);
    if (w == -1) {
        switch (errno) {
        case EINTR:
            goto Start;
        default:
            safe_close_ref(&s_Socket);
            return false;
        }
    }

    return true;
}

static
bool
Read() {
    s_Buffer.resize(0);
    char buffer[32];
Start:
    ssize_t r = read(s_Socket, buffer, sizeof(buffer));
    if (r == -1) {
        switch (errno) {
        case EINTR:
            goto Start;
        case EAGAIN:
            if (s_Buffer.empty()) {
                usleep(1000);
                goto Start;
            }
            break;
        default:
            safe_close_ref(&s_Socket);
            return false;
        }
    } else {
        if (r) {
            s_Buffer.insert(s_Buffer.end(), buffer, buffer + r);
            goto Start;
        }
    }

    return true;
}

static
void
ProcessRoute(char* input) {
    const size_t len = Sanatize(input);

    if (!len) {
        fprintf(stderr, "Command requires one arguments (the id to route to).\n");
        return;
    }

    if (Write(input, len)) {
        if (Read()) {
            s_Buffer.push_back(0);
            fprintf(stdout, "%s\n", &s_Buffer[0]);
        }
    }
}

static
int
ProcessInput(char *input) {
    Sanatize(input);

    if (strcmp("quit", input) == 0) {
        return QuitError;
    }

    if (strncmp(input, "route", 5) == 0) {
        ProcessRoute(input + 6);
    } else if (strcmp("?", input) == 0 || strcmp("help", input) == 0) {
      fprintf(stdout, s_CommandsHelp);
    } else {
        fprintf(stdout, "Unrecognized command '%s'. Type 'help' (without the quotes) to get a list of commands.\n", input);
    }

    return 0;
}
