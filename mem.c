#include "mem.h"

#include "err.h"
#include <stdlib.h>
#include <string.h>


// Manual memory management
// Currently just a wrapper over malloc and free
// Garuntees 8 byte alignment
void *mu_alloc(size_t size) {
    if (size == 0)
        return 0;

    void *m = (void *)malloc(size);

    if (m == 0)
        mu_err_nomem();

    mu_assert(sizeof m == sizeof(uint_t)); // garuntee address width
    mu_assert((7 & (uint_t)m) == 0); // garuntee alignment

    return m;
}

void *mu_realloc(void *m, size_t prev, size_t size) {
    m = realloc(m, size);

    if (m == 0)
        mu_err_nomem();

    mu_assert(sizeof m == sizeof(uint_t)); // garuntee address width
    mu_assert((7 & (uint_t)m) == 0); // garuntee alignment

    return m;
}

void mu_dealloc(void *m, size_t size) {
//    free(m);
}

