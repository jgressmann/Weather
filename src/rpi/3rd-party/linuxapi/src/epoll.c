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

#include <linuxapi/linuxapi.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MinBufferSize  4


static pthread_mutex_t s_Lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pthread_t s_EPollThread;
static int s_RefCount;
static int s_EPollFd = -1;
static epoll_callback_data* s_Callbacks;
static int s_CallbacksSize;
static volatile int s_Running = 0;
static struct epoll_event* s_Events = NULL;

static
void*
EPollThreadMain(void* arg) {
    const int epollFd = s_EPollFd;
    int count = 0, bufferSize = 0;

    s_Running = 1;

    for (;;) {
        pthread_mutex_lock(&s_Lock);
        if (!s_Events) {
            bufferSize = s_CallbacksSize >= MinBufferSize ? s_CallbacksSize : MinBufferSize;
            s_Events = (struct epoll_event*)malloc(bufferSize * sizeof(*s_Events));
            if (!s_Events) goto Exit;
        } else if (bufferSize < s_CallbacksSize) {
            s_Events = (struct epoll_event*)realloc(s_Events, s_CallbacksSize * sizeof(*s_Events));
            if (!s_Events) goto Exit;
            bufferSize = s_CallbacksSize;
        }
        pthread_mutex_unlock(&s_Lock);

        count = epoll_wait(epollFd, s_Events, bufferSize, -1);

        if (count < 0) {
            switch (errno) {
            case EINTR:
                break;
            case EBADF:
                goto Exit;
            default:
                fprintf(stderr, "epoll thread received errno %d: %s\n", errno, strerror(errno));
                goto Exit;
            }
        } else {
            int i;

            pthread_mutex_lock(&s_Lock);

            for (i = 0; i < count; ++i) {
                struct epoll_event* ev = &s_Events[i];

                if (ev->data.fd < s_CallbacksSize) {
                    if (s_Callbacks[ev->data.fd].callback) {
                        s_Callbacks[ev->data.fd].callback(s_Callbacks[ev->data.fd].ctx, ev);
                    }
                }
            }

            pthread_mutex_unlock(&s_Lock);
        }
    }

Exit:
    s_Running = 0;

    return NULL;
}

int
epoll_loop_create() {
    int error = 0;

    pthread_mutex_lock(&s_Lock);

    if (++s_RefCount == 1) {
        s_EPollFd = -1;
        s_Callbacks = NULL;
        s_CallbacksSize = 0;
        s_Events = NULL;
        s_Running = 0;

        if ((s_EPollFd = epoll_create1(O_CLOEXEC)) < 0) {
            goto Error;
        }

        if ((error = pthread_create(&s_EPollThread, NULL, EPollThreadMain, NULL)) < 0) {
            goto Error;
        }

        while (!s_Running) {
            pthread_yield();
        }
    }

    error = s_EPollFd;

Exit:
    pthread_mutex_unlock(&s_Lock);
    return error;

Error:
    error = errno;
    safe_close_ref(&s_EPollFd);
    errno = error;
    error = -1;
    goto Exit;
}

void
epoll_loop_destroy() {
    pthread_mutex_lock(&s_Lock);

    if (--s_RefCount == 0) {
        const int epollFd = s_EPollFd;
        struct epoll_event* events = s_Events;
        epoll_callback_data* callbacks = s_Callbacks;
        pthread_t thread = s_EPollThread;

        pthread_mutex_unlock(&s_Lock);
        pthread_cancel(thread);
        pthread_join(thread, NULL);

        safe_close(epollFd);
        free(events);
        free(callbacks);
    } else {
        pthread_mutex_unlock(&s_Lock);
    }
}

int
epoll_loop_set_callback(int handle, epoll_callback_data callback) {
    int error = 0;

    if (handle < 0) {
        errno = EINVAL;
        error = -1;
        goto Out;
    }

    pthread_mutex_lock(&s_Lock);

    if (!s_Callbacks) {
        size_t bytes;
        s_CallbacksSize = MinBufferSize;

        if (handle + 1 > s_CallbacksSize) {
            s_CallbacksSize = handle + 1;
        }

        bytes = sizeof(*s_Callbacks) * (size_t)s_CallbacksSize;
        s_Callbacks = (epoll_callback_data*)malloc(bytes);
        if (!s_Callbacks) {
            errno = ENOMEM;
            error = -1;
            goto Exit;
        }
        memset(s_Callbacks, 0, bytes);
    } else if (handle >= s_CallbacksSize) {
        size_t bytes;
        int previousSize;

        previousSize = s_CallbacksSize;
        s_CallbacksSize = (s_CallbacksSize * 17) / 10; /* x 1.68 */
        if (handle + 1 > s_CallbacksSize) {
            s_CallbacksSize = handle + 1;
        }

        bytes = sizeof(*s_Callbacks) * (size_t)s_CallbacksSize;
        s_Callbacks = (epoll_callback_data*)realloc(s_Callbacks, bytes);
        if (!s_Callbacks) {
            errno = ENOMEM;
            error = -1;
            goto Exit;
        }
        memset(s_Callbacks + previousSize, 0, sizeof(*s_Callbacks) * (size_t)(s_CallbacksSize - previousSize));
    }

    s_Callbacks[handle] = callback;

Exit:
    pthread_mutex_unlock(&s_Lock);

Out:
    return error;
}

int
epoll_loop_get_fd() {
    return s_EPollFd;
}
