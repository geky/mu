/* 
 *  Memory management
 */

#ifdef MU_DEF
#ifndef MU_MEM_DEF
#define MU_MEM_DEF

#include "mu.h"


// Reference type as word type aligned to 8 bytes
typedef mu_aligned unsigned int ref_t;


#endif
#else
#ifndef MU_MEM_H
#define MU_MEM_H
#define MU_DEF
#include "mem.h"
#include "err.h"
#undef MU_DEF

#include <stdlib.h>


// Manual memory management
// simple wrapper over malloc and free if available
// returns 0 when size == 0
void *mu_alloc(size_t size, eh_t *eh);
void *mu_realloc(void *, size_t prev, size_t size, eh_t *eh);
void mu_dealloc(void *, size_t size);


// Garbage collected memory based on reference counting
// Each block of memory prefixed with ref_t reference
// count. Deallocated immediately when ref hits zero.
// It is up to the user to avoid cyclic dependencies.
void *ref_alloc(size_t size, eh_t *eh);

void ref_dealloc(void *m, size_t size);


mu_inline void ref_inc(void *m) {
    ref_t *ref = (ref_t*)(~0x7 & (uint32_t)m);

    if (*ref != 0)
        (*ref)++;
}

mu_inline void ref_dec(void *m, void (*dtor)(void*)) {
    ref_t *ref = (ref_t*)(~0x7 & (uint32_t)m);

    if (*ref != 0)
        if (--(*ref) == 0)
            dtor(ref + 1);
}


#endif
#endif
