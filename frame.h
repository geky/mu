/*
 * Frames for passing multiple variables
 */

#ifndef MU_FRAME_H
#define MU_FRAME_H
#include "mu.h"
#include "types.h"


// Number of elements that can be stored in a frame.
// Passing more than 'MU_FRAME' elements simply stores
// a full table in the first element.
//
// For function calls, the frame count is split into two
// nibbles for arguments and return counts. If either
// is greater than 'MU_FRAME' a table can be passed instead
// by specifying 0xf
#define MU_FRAME 4


// Conversion between different frame types
void mu_fto(frame_t dc, frame_t sc, mu_t *frame);

// Number of elements in frame
mu_inline uint_t mu_fcount(frame_t fc) {
    return (fc == 0xf) ? 1 : fc;
}


#endif
