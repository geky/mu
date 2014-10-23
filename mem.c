#include "mem.h"
#include "var.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// Manual memory management
// Currently just a wrapper over malloc and free
// Garuntees 8 byte alignment
void *mu_alloc(size_t size, eh_t *eh) {
    void *m;
    
    if (size == 0)
        return 0;

    m = (void *)malloc(size);

    if (m == 0)
        err_nomem(eh);

    assert(sizeof m == sizeof(uint32_t)); // garuntee address width
    assert((0x7 & (uint32_t)m) == 0); // garuntee alignment

    return m;
}

void *mu_realloc(void *m, size_t prev, size_t size, eh_t *eh) {
    m = realloc(m, size);

    if (m == 0)
        err_nomem(eh);

    assert(sizeof m == sizeof(uint32_t)); // garuntee address width
    assert((0x7 & (uint32_t)m) == 0); // garuntee alignment

    return m;
}

void mu_dealloc(void *m, size_t size) {
//    free(m);
}


// Garbage collected memory based on reference counting
// Each block of memory prefixed with ref_t reference
// count. Deallocated immediately when ref hits zero.
// It is up to the user to avoid cyclic dependencies.
void *ref_alloc(size_t size, eh_t *eh) {
    ref_t *m = mu_alloc(sizeof(ref_t) + size, eh);

    // start with a count of 1
    *m = 1;
    return m + 1;
}

void ref_dealloc(void *m, size_t size) {
    mu_dealloc(m, sizeof(ref_t) + size);
}
