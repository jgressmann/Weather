/* See buffer.c for license information */

#ifndef BUFFER_94993292_H
#define BUFFER_94993292_H

#include <stddef.h>
#include <stdint.h>
#include <toe/stack.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    aba_ptr next;
    uintptr_t flags;
    unsigned char* beg;
    unsigned char* end;
    unsigned char* cap;
} buffer;

/* Sets the minium buffer size.
 *
 * Default minimum size is 64 bytes (1 cache line)
 *
 * Param bytes: minimum size in bytes.
 *  A value of 0 sets the default size.
 */
TOE_DLL_API
void
buf_set_min_size(size_t bytes);

/* Sets the size in bytes to which the buffer cache can grow.
 *
 * Defaults to 0 which is unlimited.
 *
 * Param bytes: size in bytes of the cache
 *  A value of 0 sets unlimited caching.
 */
TOE_DLL_API
void
buf_set_cache_limit(size_t bytes);

/* Sets the size of memory block to allocate the buffer structures are exhausted.
 *
 * Defaults to 0 which is equivalent to sizeof(buffer).
 *
 * Param bytes: size in bytes of the memory block.
 */
TOE_DLL_API
void
buf_set_batch_size(size_t bytes);

/* Creates a buffer of the given size.
 *
 * Returns NULL if malloc fails
 * */
TOE_DLL_API
buffer*
buf_alloc(size_t bytes);


/* Resizes the buffer to the specified size
 *
 * Returns 1 on success, 0 on failure.
 * */
TOE_DLL_API
int
buf_grow(buffer* buf, size_t bytes);

/* Returns the buffer to the cache */
TOE_DLL_API
void
buf_free(buffer* buf);

/* Clears the buffer cache
 *
 * NOTE: Use this with extreme caution if your program is multithreaded!
 * NOTE: You *may* get random crashs. You have been warned.
 */
TOE_DLL_API
void
buf_clear_cache(void);

/* Evaluates to the capacity of a buffer */
#define buf_size(buf) ((size_t)((buf)->cap - (buf)->beg))
/* Evaluates to the used bytes of a buffer. Always <= buffer capacity */
#define buf_used(buf) ((size_t)((buf)->end - (buf)->beg))
/* Evaluates to the number of unused bytes left in buffer. Always in range [0, capacity] */
#define buf_left(buf) ((size_t)((buf)->cap - (buf)->end))
/* Resizes the buffer to fit size bytes */
#define buf_resize(buf, size) ((buf_size(buf) < (size)) ? buf_grow(buf, size) : 1)
/* Clears the buffer i.e. sets used to 0 */
#define buf_clear(buf) buf->end = buf->beg
/* Reserves space for size bytes at then end */
#define buf_reserve(buf, size) buf_resize(buf, buf_used(buf) + size)

#ifdef __cplusplus
}
#endif

#endif /* BUFFER_94993292_H */

