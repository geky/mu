/*
 * Common definitions
 */

#ifndef MU_H
#define MU_H

#include <stdint.h>
#include <stdbool.h>


// Definition of macro-like inlined functions
#define mu_inline static inline __attribute__((always_inline))

// Definition of non-returning functions
#define mu_noreturn __attribute__((noreturn))

// Definition of alignment for Mu types
#define mu_aligned __attribute__((aligned(8)))

// Definition of const functions
// TODO replace most of these with actual const allocations
#define mu_const __attribute__((const))

// Builtins for the likelyness of branches
#define mu_likely(x) __builtin_expect(x, 1)
#define mu_unlikely(x) __builtin_expect(x, 0)

// Builtin for an unreachable point in code
#define mu_unreachable() __builtin_unreachable()

// Builtin for the next power of two
mu_inline uint32_t mu_npw2(uint32_t i) {
    return i ? 1 << (32-__builtin_clz(i - 1)) : 0;
}


// Definition of Mu specific assert function
#ifdef MU_DEBUG
#include <assert.h>
#define mu_assert(x) assert(x)
#else
#define mu_assert(x)
#endif


#endif
