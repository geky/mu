/*
 * Common definitions
 */

#ifndef MU_H
#define MU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


// Definitions of the basic types used in Mu
// Default and half-sized integer types
typedef int8_t intq_t;
typedef uint8_t uintq_t;
typedef int16_t inth_t;
typedef uint16_t uinth_t;
typedef int32_t int_t;
typedef uint32_t uint_t;

// Length type is half the size of normal ints
// to reduce space consumption
typedef uinth_t len_t;

// Largest storeable length
#define MU_MAXLEN ((len_t)~0)

// Smallest allocatable size
#define MU_MINALLOC (4*sizeof(void *))


// Definition of macro-like inlined functions
#define mu_inline static inline __attribute__((always_inline))

// Definition of non-returning functions
#define mu_noreturn __attribute__((noreturn))

// Definition of alignment for Mu types
#define mu_aligned __attribute__((aligned(8)))

// Definition of thread local variables
#define mu_thread __thread

// Definition of const functions
// TODO replace most of these with seperate const allocations
#define mu_const __attribute__((const))

// Builtin for a potentially unused variable
#define mu_unused __attribute__((unused))

// Builtins for the likelyness of branches
#define mu_likely(x) __builtin_expect(x, 1)
#define mu_unlikely(x) __builtin_expect(x, 0)

// Builtin for an unreachable point in code
#define mu_unreachable() __builtin_unreachable()

// Builtin for the next power of two
#define mu_npw2(x) (8*sizeof(uint_t) - __builtin_clz((x)-1))


// Definition of Mu specific assert function
#ifdef MU_DEBUG
#include <assert.h>
#define mu_assert(x) assert(x)
#else
#define mu_assert(x)
#endif


#endif
