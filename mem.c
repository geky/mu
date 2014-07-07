#include "mem.h"
#include "var.h"

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
//    free(m);
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
    // garuntee alignment
    assert((0x7 & (uint32_t)m) == 0);

    // start with count of 1
    *m = 1;

    // return the data following the reference
    return m + 1;
}

