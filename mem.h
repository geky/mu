/*
 *  Memory management
 */

#ifndef MU_MEM_H
#define MU_MEM_H
#include "mu.h"


// Reference type as word type aligned to 8 bytes
typedef mu_aligned uinth_t ref_t;

// Smallest allocatable size
#define MU_MINALLOC (4*sizeof(void*))


// Manual memory management
// simple wrapper over malloc and free if available
// returns 0 when size == 0
void *mu_alloc(size_t size);
void *mu_realloc(void *, size_t prev, size_t size);
void mu_dealloc(void *, size_t size);


// Garbage collected memory based on reference counting
// Each block of memory starts with a ref_t reference count.
// Deallocated immediately when ref hits zero.
// A reference count of zero indicates a constant variable
// which could be statically allocated. This nicely handles
// overflow allowing a small reference count size.
// It is up to the user to avoid cyclic dependencies.
mu_inline void *ref_alloc(size_t size) {
    ref_t *ref = mu_alloc(size);
    *ref = 1;

    return ref;
}

mu_inline void ref_dealloc(void *m, size_t size) {
    mu_dealloc((ref_t *)(~7 & (uint_t)m), size);
}

mu_inline void ref_inc(void *m) {
    ref_t *ref = (ref_t *)(~7 & (uint_t)m);

    if (*ref != 0)
        (*ref)++;
}

mu_inline bool ref_dec(void *m) {
    ref_t *ref = (ref_t *)(~7 & (uint_t)m);

    if (*ref != 0) {
        (*ref)--;

        if (*ref == 0)
            return true;
    }

    return false;
}


#endif
