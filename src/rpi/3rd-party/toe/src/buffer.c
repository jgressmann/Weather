/* The MIT License (MIT)
 *
 * Copyright (c) 2015 Jean Gressmann <jean@0x42.de>
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


#include <toe/buffer.h>


#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#if defined(_WIN64) || defined(_WIN32)
#   include <windows.h>
#   define membar() MemoryBarrier()
#   define INLINE __forceinline
#   define cas1(ptr, new, old) (InterlockedCompareExchange((LONG volatile *)ptr, *((LONG*)old), *((LONG*)new)) == *((LONG*)old))
#elif defined(__GNUC__)
#   define INLINE inline
#   define cas1(ptr, new, old) __sync_bool_compare_and_swap((__int32_t*)ptr, *((__int32_t*)old), *((__int32_t*)new))
#endif


#define FLAG_DELETE 1

static stack_head s_Stack = STACK_HEAD_INITIALIZER(0);
#define DEFAULT_MIN_SIZE 64 /* one cache line on Intel */
static size_t s_MinSize = DEFAULT_MIN_SIZE;
static size_t s_Limit = 0;
static volatile size_t s_Use = 0;
static size_t s_BatchSize = 0;


static
INLINE
void
Push(buffer* node) {
    stack_push(&s_Stack, node);
}

static
INLINE
buffer*
Pop() {
    return (buffer*)stack_pop(&s_Stack);
}

static
INLINE
size_t
Max(size_t lhs, size_t rhs) {
    return lhs < rhs ? rhs : lhs;
}

static
size_t
AtomicAdd(size_t volatile * counter, intptr_t value) {
    size_t old, _new;
    do {
        old = *counter;
        _new = old + value;
    } while (!cas1(counter, &_new, &old));

    return _new;
}

#if defined(__GNUC__)
__attribute__((constructor))
#endif
static void Setup() {
   atexit(buf_clear_cache);
}

extern
buffer*
buf_alloc(size_t bytes) {
    buffer* buf = Pop(&s_Stack);

    if (!buf) {
        size_t bytesToAllocate = s_BatchSize;
        size_t count = bytesToAllocate / sizeof(buffer);
        if (!count) {
            bytesToAllocate = sizeof(buffer);
            count = 1;
        }

        buf = (buffer*)malloc(bytesToAllocate);
        if (!buf) {
            return NULL;
        }

        memset(buf, 0, bytesToAllocate);

        for (size_t i = 1; i < count; ++i) {
            Push(&buf[i]);
        }

        buf->flags = FLAG_DELETE;
    }

    AtomicAdd(&s_Use, -(intptr_t)buf_size(buf));

    if (!buf_resize(buf, bytes)) {
        AtomicAdd(&s_Use, (intptr_t)buf_size(buf));
        Push(buf);
        return NULL;
    }

    //fprintf(stderr, "out %p %p\n", buf, buf->cap);

    return buf;
}

extern
int
buf_grow(buffer* buf, size_t bytes) {
    size_t oldSize, oldUsed, newSize;

    assert(buf);

    oldUsed = buf_used(buf);
    oldSize = buf_size(buf);
    newSize = Max((oldSize * 168) / 100, Max(bytes, s_MinSize));
    buf->beg = (unsigned char*)realloc(buf->beg, newSize);
    if (!buf->beg) {
        buf->cap = NULL;
        buf->end = NULL;
        return 0;
    }

    buf->end = buf->beg + oldUsed;
    buf->cap = buf->beg + newSize;

    return 1;
}

extern
void
buf_free(buffer* buf)
{
    assert(buf);

    //fprintf(stderr, "in  %p %p\n", buf, buf->cap);

    if (s_Limit && s_Use >= s_Limit) {
        free(buf->beg);
        buf->beg = NULL;
        buf->end = NULL;
        buf->cap = NULL;
    } else {
        buf_clear(buf);
        AtomicAdd(&s_Use, (intptr_t)buf_size(buf));
    }

    Push(buf);
}

extern
void
buf_set_min_size(size_t bytes) {
    s_MinSize = bytes ? bytes : DEFAULT_MIN_SIZE;
}

extern
void
buf_set_cache_limit(size_t bytes) {
    s_Limit = bytes;
}

void
buf_clear_cache() {
    buffer* allocatedHead = NULL;
    buffer* buf = NULL;
    while ((buf = Pop()) != NULL) {
        free(buf->beg);
        if (buf->flags & FLAG_DELETE) {
            buf->next.Ptr = allocatedHead;
            allocatedHead = buf;
        }
    }

    while (allocatedHead) {
        buf = allocatedHead;
        allocatedHead = (buffer*)allocatedHead->next.Ptr;
        free(buf);
    }

    s_Use = 0;
}


void
buf_set_batch_size(size_t bytes) {
    s_BatchSize = bytes;
}
