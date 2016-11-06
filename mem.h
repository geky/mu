/*
 * Memory management
 */

#ifndef MU_MEM_H
#define MU_MEM_H
#include "config.h"


// Reference type as word type aligned to 8 bytes
typedef mu_aligned(8) muinth_t mref_t;

// Smallest allocatable size
#define MU_MINALLOC (4*sizeof(muint_t))


// Manual memory management
// simple wrapper over malloc and free if available
// returns 0 when size == 0
void *mu_alloc(muint_t size);
void mu_dealloc(void *, muint_t size);


// Garbage collected memory based on reference counting
// Each block of memory starts with a mu_ref_t reference count.
// Deallocated immediately when ref hits zero.
// A reference count of zero indicates a constant variable
// which could be statically allocated. This nicely handles
// overflow allowing a small reference count size.
// It is up to the user to avoid cyclic dependencies.
mu_inline void *mu_ref_alloc(muint_t size) {
    mref_t *ref = mu_alloc(size);
    *ref = 1;

    return ref;
}

mu_inline void mu_ref_dealloc(void *m, muint_t size) {
    mu_dealloc((mref_t *)(~7 & (muint_t)m), size);
}

mu_inline void mu_ref_inc(void *m) {
    mref_t *ref = (mref_t *)(~7 & (muint_t)m);

    if (*ref != 0) {
        (*ref)++;
    }
}

mu_inline bool mu_ref_dec(void *m) {
    mref_t *ref = (mref_t *)(~7 & (muint_t)m);

    if (*ref != 0) {
        (*ref)--;

        if (*ref == 0) {
            return true;
        }
    }

    return false;
}


#endif
