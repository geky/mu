#include "mem.h"

#include "mu.h"
#include "sys.h"
#include "str.h"


// Uses malloc/free if MU_MALLOC is defined
#ifdef MU_MALLOC
extern void *malloc(muint_t);
extern void free(void *);
#define mu_sys_alloc(size) malloc(size)
#define mu_sys_dealloc(m, size) free(m)
#endif


// Manual memory management
// Currently just a wrapper over malloc and free
// Garuntees 8 byte alignment
void *mu_alloc(muint_t size) {
    if (size == 0) {
        return 0;
    }

#ifdef MU_DEBUG
    size += sizeof(muint_t);
#endif

    void *m = mu_sys_alloc(size);

    if (m == 0) {
        const char *message = "out of memory";
        mu_error(message, strlen(message));
    }

    mu_assert(sizeof m == sizeof(muint_t)); // garuntee address width
    mu_assert((7 & (muint_t)m) == 0); // garuntee alignment

#ifdef MU_DEBUG
    size -= sizeof(muint_t);
    *(muint_t*)&((char*)m)[size] = size;
#endif

    return m;
}

void mu_dealloc(void *m, muint_t size) {
#ifdef MU_DEBUG
    mu_assert(!m || *(muint_t*)&((char*)m)[size] == size);
#endif

    mu_sys_dealloc(m, size);
}

