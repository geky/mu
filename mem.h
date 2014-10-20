/* 
 *  Memory management
 */

#ifndef V_MEM
#define V_MEM

#include <stdint.h>
#include <string.h>

#include "err.h"


// Reference type as word type
// Aligned to 8 bytes
typedef unsigned int ref_t 
__attribute__((aligned(8)));


// Manual memory management
// simple wrapper over malloc and free if available
// returns 0 when size == 0
void *v_alloc(size_t size, veh_t *veh);
void *v_realloc(void *, size_t prev, size_t size, veh_t *veh);
void v_dealloc(void *, size_t size);


// Garbage collected memory based on reference counting
// Each block of memory prefixed with ref_t reference
// count. Deallocated immediately when ref hits zero.
// It is up to the user to avoid cyclic dependencies.
void *vref_alloc(size_t size, veh_t *eh);

void vref_dealloc(void *m, size_t size);


static inline void vref_inc(void *m) {
    ref_t *ref = (ref_t*)(~0x7 & (uint32_t)m);
    (*ref)++;
}

static inline void vref_dec(void *m, void (*dtor)(void*)) {
    ref_t *ref = (ref_t*)(~0x7 & (uint32_t)m);

    if (--(*ref) == 0)
        dtor(ref + 1);
}


#endif
