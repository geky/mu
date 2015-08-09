#include "frame.h"

#include "num.h"
#include "tbl.h"


// Conversion between different frame types
// Supports inplace conversion
void mu_fto(frame_t dc, frame_t sc, mu_t *frame) {
    if (dc != 0xf && sc != 0xf) {
        for (uint_t i = dc; i < sc; i++)
            mu_dec(frame[i]);

        for (uint_t i = sc; i < dc; i++)
            frame[i] = mnil;

    } else if (dc != 0xf) {
        mu_t t = *frame;

        for (uint_t i = 0; i < dc; i++)
            frame[i] = tbl_lookup(t, muint(i));

        tbl_dec(t);
    } else if (sc != 0xf) {
        mu_t t = tbl_create(sc);

        for (uint_t i = 0; i < sc; i++)
            tbl_insert(t, muint(i), frame[i]);

        *frame = t;
    }
}
