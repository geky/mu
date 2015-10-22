#include "mem.h"

#include "mu.h"
#include "sys.h"
#include "str.h"


// Uses malloc/free if MU_MALLOC is defined
#ifdef MU_MALLOC
extern void *malloc(muint_t);
extern void free(void *);
#define sys_alloc(size) malloc(size)
#define sys_dealloc(m, size) free(m)
#endif


// Defined as constant since at this point Mu is
// in fact out of memory
#define MU_OOM mu_oom()
MSTR(mu_oom, "out of memory")


// Manual memory management
// Currently just a wrapper over malloc and free
// Garuntees 8 byte alignment
void *mu_alloc(muint_t size) {
    if (size == 0)
        return 0;

#ifdef MU_DEBUG
    size += sizeof(muint_t);
#endif

    void *m = sys_alloc(size);

    if (m == 0)
        mu_error(MU_OOM);

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

    sys_dealloc(m, size);
}

