#include "frame.h"

#include "num.h"
#include "tbl.h"


// Varargs frame handling
void mu_toframe(frame_t c, mu_t *frame, va_list args) {
    if (c > MU_FRAME) {
        *frame = va_arg(args, mu_t);
    } else {
        for (uint_t i = 0; i < c; i++)
            frame[i] = va_arg(args, mu_t);
    }
}

mu_t mu_fromframe(frame_t c, mu_t *frame, va_list args) {
    if (c <= MU_FRAME) {
        for (uint_t i = 1; i < c; i++)
            *va_arg(args, mu_t *) = frame[i];
    }

    return c ? *frame : mnil;
}


// Conversion between different frame types
// Supports inplace conversion
void mu_fconvert(frame_t dc, mu_t *dframe, frame_t sc, mu_t *sframe) {
    if (dc > MU_FRAME && sc > MU_FRAME) {
        *dframe = *sframe;
    } else if (dc > MU_FRAME) {
        mu_t t = tbl_create(sc);

        for (uint_t i = 0; i < sc; i++)
            tbl_insert(t, muint(i), sframe[i]);

        *dframe = t;
    } else if (sc > MU_FRAME) {
        mu_t t = *sframe;

        for (uint_t i = 0; i < dc; i++)
            dframe[i] = tbl_lookup(t, muint(i));

        tbl_dec(t);
    } else {
        for (uint_t i = 0; i < sc && i < dc; i++)
            dframe[i] = sframe[i];

        for (uint_t i = dc; i < sc; i++)
            mu_dec(sframe[i]);

        for (uint_t i = sc; i < dc; i++)
            dframe[i] = mnil;
    }
}
