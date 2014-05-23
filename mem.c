#include "mem.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// Manual memory management
// Currently just a wrapper over malloc and free
void *valloc(size_t size) {
    void *m;
    
    if (size == 0) return 0;

    m = (void *)malloc(size);

    assert(m != 0); // TODO error on out of memory

    return m;
}

void vdealloc(void *m) {
    free(m);
}


// Garbage collected memory based on reference counting
// Each block of memory prefixed with ref_t reference
// count. Deallocated immediately when ref hits zero.
// It is up to the user to avoid cyclic dependencies.
// Reference is garunteed to be aligned to 8 bytes.
// References ignored if 3rd bit is not set.
void *vref_alloc(size_t size) {
    ref_t *m = valloc(sizeof(ref_t) + size);

    // garuntee address width
    assert(sizeof m == sizeof(uint32_t));
    // garuntee alignement
    assert((0x7 & (uint32_t)m) == 0);

    // start with count of 1
    *m = 1;

    // return the data following the referece
    // also garuntees that bit 3 is set
    return m + 1;
}

void vref_inc(void *m) {
    uint32_t bits = (uint32_t)m;

    if (bits & 0x4) {
        ref_t *ref = (ref_t *)(~0x7 & bits);

        if (*ref)
            (*ref)++;
    }
}

void vref_dec(void *m) {
    uint32_t bits = (uint32_t)m;

    if (bits & 0x4) {
        ref_t *ref = (ref_t *)(~0x7 & bits);

        if (*ref) {
            (*ref)--;

            if (*ref == 0) {
                // TODO check for special var cases

                vdealloc(ref);
            }
        }
    }
}

