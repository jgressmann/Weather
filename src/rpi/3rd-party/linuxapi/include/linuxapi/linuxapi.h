#ifndef LINUXAPI_H
#define LINUXAPI_H

#include <sys/epoll.h>
#include <sys/epoll.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Closes the handle and retries on EINTR */
extern void safe_close(int handle);

/* Closes the handle and then sets it to -1 */
extern void safe_close_ref(int* handle);

/* Opens the file and retries on EINTR */
extern int safe_open(const char* path, int flags, ...);

/* Callback function type for epoll events */
typedef void (*epoll_callback_t)(void* ctx, struct epoll_event* ev);

/* Structure for callback data */
typedef struct _epoll_callback_data {
    void* ctx;
    epoll_callback_t callback;
}
epoll_callback_data;

/* Creates the epoll event loop
 *
 * Return: The epoll file descriptor on success, else -1. Use errno for details.
 *
 */
extern int epoll_loop_create();

/* Returns the epoll file descriptor. The value is -1 if no loop exists. */
extern int epoll_loop_get_fd();

/* Decrement the loop reference cout
 *
 * If the ref count drops to 0 the epoll instance and other resources related
 * to the loop will be freed.
 *
 */
extern void epoll_loop_destroy();

/* Sets an event callback for a file descriptor
 *
 * Return: 0 on success; otherwise -1. Use errno to get details.
 *
 */
int epoll_loop_set_callback(int handle, epoll_callback_data callback);


#ifdef __cplusplus
}
#endif

#endif /* LINUXAPI_H */

