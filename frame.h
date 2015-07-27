/*
 * Variable types and definitions
 */

#ifndef MU_FRAME_H
#define MU_FRAME_H
#include "mu.h"
#include "types.h"
#include <stdarg.h>


// Number of elements that can be stored in a frame.
// Passing more than 'MU_FRAME' elements simply stores
// a full table in the first element.
//
// For function calls, the frame count is split into two
// nibbles for arguments and return counts. If either
// is greater than 'MU_FRAME' a table can be passed instead
// by specifying 0xf
#define MU_FRAME 4


// Varargs frame handling
void mu_toframe(frame_t fc, mu_t *frame, va_list args);
mu_t mu_fromframe(frame_t fc, mu_t *frame, va_list args);

// Conversion between different frame types
void mu_fconvert(frame_t dc, mu_t *dframe, frame_t sc, mu_t *sframe);


#endif
