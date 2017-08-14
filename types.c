#include "types.h"
#include "mu.h"


// Mu type destruction
void mu_destroy(mu_t m) {
    extern void mu_str_destroy(mu_t);
    extern void mu_buf_destroy(mu_t);
    extern void mu_buf_destroydtor(mu_t);
    extern void mu_tbl_destroy(mu_t);
    extern void mu_fn_destroy(mu_t);

    switch (mu_gettype(m)) {
        case MTSTR:  mu_str_destroy(m);     return;
        case MTBUF:  mu_buf_destroy(m);     return;
        case MTCBUF: mu_buf_destroydtor(m); return;
        case MTTBL:  mu_tbl_destroy(m);     return;
        case MTFN:   mu_fn_destroy(m);      return;
        default:     mu_unreachable;
    }
}


// Frame operations
void mu_frameconvert(mcnt_t sc, mcnt_t dc, mu_t *frame) {
    if (dc != 0xf && sc != 0xf) {
        for (muint_t i = dc; i < sc; i++) {
            mu_dec(frame[i]);
        }

        for (muint_t i = sc; i < dc; i++) {
            frame[i] = 0;
        }
    } else if (dc != 0xf) {
        mu_t t = *frame;

        for (muint_t i = 0; i < dc; i++) {
            frame[i] = mu_tbl_lookup(t, mu_num_fromuint(i));
        }

        mu_tbl_dec(t);
    } else if (sc != 0xf) {
        mu_t t = mu_tbl_create(sc);

        for (muint_t i = 0; i < sc; i++) {
            mu_tbl_insert(t, mu_num_fromuint(i), frame[i]);
        }

        *frame = t;
    }
}

