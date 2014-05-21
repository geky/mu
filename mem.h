/* 
 *  Memory management
 */

#ifndef V_MEM
#define V_MEM

#include <string.h>

// Reference type as word type
// Aligned to 8 bytes
typedef unsigned int ref_t __attribute__((aligned(8)));


// Manual memory management
// simple wrapper over malloc and free if available
// returns 0 when size == 0
void *valloc(size_t size);
void vdealloc(void *);


// Garbage collected memory based on reference counting
// Each block of memory prefixed with ref_t reference
// count. Deallocated immediately when ref hits zero.
// It is up to the user to avoid cyclic dependencies.
// Reference is garunteed to be aligned to 8 bytes.
// References ignored if 3rd bit is not set.
void *vref_alloc(size_t size);
void vref_inc(void *ref);
void vref_dec(void *ref);


#endif
