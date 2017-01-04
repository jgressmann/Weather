
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


#include <toe/stack.h>


#include <stdint.h>
#include <assert.h>

#if defined(_WIN64) || defined(_WIN32)
#   include <windows.h>
#   define membar() MemoryBarrier()
#   define INLINE __forceinline
#   if defined(_WIN64)
#       define cas2(ptr, new, old) InterlockedCompareExchange128((LONGLONG volatile *)ptr, (*((LONGLONG*)new) >> 64, *((LONGLONG*)new), (LONGLONG*)old)
#   else
#       define cas2(ptr, new, old) (InterlockedCompareExchange64((LONGLONG volatile *)ptr, *((LONGLONG*)new), *((LONGLONG*)old)) == *((LONGLONG*)old))
#   endif
#   define cas1(ptr, new, old) (InterlockedCompareExchange((LONG volatile *)ptr, *((LONG*)old), *((LONG*)new)) == *((LONG*)old))
#   define yield() Sleep(0)
#elif defined(__GNUC__)
#   include <pthread.h>
#   define membar() __sync_synchronize()
#   define INLINE inline
#   if __WORDSIZE == 32
#       define cas2(ptr, new, old) __sync_bool_compare_and_swap((__int64_t*)ptr, *((__int64_t*)old), *((__int64_t*)new))
#   else
#       define cas2(ptr, new, old) __sync_bool_compare_and_swap((__int128*)ptr, *((__int128_t*)old), *((__int128_t*)new))
#   endif
#   define cas1(ptr, new, old) __sync_bool_compare_and_swap((__int32_t*)ptr, *((__int32_t*)old), *((__int32_t*)new))
#   define yield() pthread_yield()
#endif


/* assert sizeof aba_ptr == 2 * sizeof(void*) */
typedef char AssertSizeofAbaPtrEqualsTwiceSizeofVoidStar[sizeof(aba_ptr) == 2 * sizeof(void*) ? 1 : -1];

void
stack_init(
    stack_head* head,
    size_t offset) {

    assert(head);
    head->Head.Aba = 0;
    head->Head.Ptr = 0;
    head->Offset = offset;
}

#ifdef SINGLECORE
void
stack_push(
    stack_head* head,
    void* node) {

    aba_ptr* nodeNext;

    assert(head);
    assert(node);

    uintptr_t n = 1, o = 0;
    while (!cas1(&head->Head.Aba, &n, &o)) {
        yield();
    }

    nodeNext = (aba_ptr*)(((uint8_t*)node) + head->Offset);
    nodeNext->Ptr = head->Head.Ptr;
    head->Head.Ptr = node;

    n = 0, o = 1;
    cas1(&head->Head.Aba, &n, &o);
}
#else
void
stack_push(
    stack_head* head,
    void* node) {

    assert(head);
    assert(node);

    volatile aba_ptr* nodeNext = (aba_ptr*)(((uint8_t*)node) + head->Offset);
    aba_ptr oldHead, newHead;

    do {
        membar();

        oldHead = head->Head;

        nodeNext->Ptr = oldHead.Ptr;

        membar(); /* make write visible */

        newHead.Aba = oldHead.Aba + 1;
        newHead.Ptr = node;
    } while (!cas2(&head->Head, &newHead, &oldHead));
}
#endif

#ifdef SINGLECORE
void*
stack_pop(
    stack_head* head) {

    assert(head);

    void* result = NULL;

    uintptr_t n = 1, o = 0;
    while (!cas1(&head->Head.Aba, &n, &o)) {
        yield();
    }

    if (head->Head.Ptr) {
        result = head->Head.Ptr;
        head->Head.Ptr = ((volatile aba_ptr*)(((uint8_t*)result) + head->Offset))->Ptr;
    }

    n = 0, o = 1;
    cas1(&head->Head.Aba, &n, &o);

    return result;
}
#else
void*
stack_pop(
    stack_head* head) {

    assert(head);

    void* result;
    aba_ptr oldHead, newHead;

    do {
        membar();

        oldHead = head->Head;

        if (!oldHead.Ptr) {
            result = NULL;
            break;
        }

        result = oldHead.Ptr;

        newHead.Aba = oldHead.Aba + 1;
        newHead.Ptr = ((volatile aba_ptr*)(((uint8_t*)result) + head->Offset))->Ptr;
    } while (!cas2(&head->Head, &newHead, &oldHead));

    return result;
}
#endif

