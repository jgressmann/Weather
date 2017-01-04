/* See stack.c for license information */

#ifndef STACK_44994_H
#define STACK_44994_H

#include <stddef.h>
#include <stdint.h>
#include <toe/dll.h>

#if defined( _MSC_VER)
#   if defined(_WIN64)
#      define ALIGN(x) __declspec(align(16))
#   else
#      define ALIGN(x) __declspec(align(8))
#   endif
#elif defined(__GNUC__)
#   define ALIGN(x) __attribute__((aligned (x)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef ALIGN(2 * sizeof(void*)) struct {
    void* Ptr;
    uintptr_t Aba;
} aba_ptr;


/* Head of a lock-free stack */
typedef struct  {
    volatile aba_ptr Head;
    size_t Offset;
} stack_head;

/* Macro for static initialization of a stack head */
#define STACK_HEAD_INITIALIZER(offset) { { 0, 0 }, offset }

/* Initializes a stack head
 *
 * Param head: Pointer to the stack head to
 *  initialize
 * Param offset: Offset into the node structure
 *  at which the aba_ptr for the next entry is located.
 */
TOE_DLL_API
void
stack_init(stack_head* head, size_t offset);

/* Pushes a node onto the stack
 *
 * Param head: Pointer to initialized stack head
 * Param node: Pointer to the node to push
 */
TOE_DLL_API
void
stack_push(stack_head* head, void* node);

/* Pops a node from the stack
 *
 * Return: Pointer to a node or NULL if the stack
 *  is empty.
 */
TOE_DLL_API
void*
stack_pop(stack_head* head);

#ifdef __cplusplus
}
#endif

#endif /* STACK_44994_H */

