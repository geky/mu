#include "mem.h"

#include "err.h"

#include <stdlib.h>
#include <string.h>


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

    mu_assert(sizeof m == sizeof(uint_t)); // garuntee address width
    mu_assert((7 & (uint_t)m) == 0); // garuntee alignment

    return m;
}

void *mu_realloc(void *m, size_t prev, size_t size, eh_t *eh) {
    m = realloc(m, size);

    if (m == 0)
        err_nomem(eh);

    mu_assert(sizeof m == sizeof(uint_t)); // garuntee address width
    mu_assert((7 & (uint_t)m) == 0); // garuntee alignment

    return m;
}

void mu_dealloc(void *m, size_t size) {
//    free(m);
}

