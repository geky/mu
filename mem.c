#include "mem.h"
#include "var.h"

#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

// Manual memory management
// Currently just a wrapper over malloc and free
// Garuntees 8 byte alignment
void *valloc(size_t size) {
    void *m;
    
    if (size == 0) return 0;

    m = (void *)malloc(size);

    assert(m != 0); // TODO error on out of memory
    assert(sizeof m == sizeof(uint32_t)); // garuntee address width
    assert((0x7 & (uint32_t)m) == 0); // garuntee alignment

    return m;
}

void vdealloc(void *m, size_t size) {
//    free(m);
}

