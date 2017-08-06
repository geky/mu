/*
 * Mu config
 */

#ifndef MU_CONFIG_H
#define MU_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


// Determine if mu is 32-bit or 64-bit
#if !defined(MU32) && !defined(MU64)
#if UINT32_MAX == UINTPTR_MAX
#define MU32
#elif UINT64_MAX == UINTPTR_MAX
#define MU64
#else
#error "Unspecified word size for Mu"
#endif
#endif

// Definitions of the basic types used in Mu
// Default and half-sized integer types
#ifdef MU32
typedef int8_t   mintq_t;
typedef uint8_t  muintq_t;
typedef int16_t  minth_t;
typedef uint16_t muinth_t;
typedef int32_t  mint_t;
typedef uint32_t muint_t;
typedef float    mfloat_t;
#else
typedef int16_t  mintq_t;
typedef uint16_t muintq_t;
typedef int32_t  minth_t;
typedef uint32_t muinth_t;
typedef int64_t  mint_t;
typedef uint64_t muint_t;
typedef double   mfloat_t;
#endif

// Flags for Mu options
//#define MU_DEBUG
#define MU_MALLOC
//#define MU_DISASSEMBLE
//#define MU_COMPUTED_GOTO


// Definition of macro-like inlined functions
#ifndef MU_DEBUG
#define mu_inline static inline __attribute__((always_inline))
#else
#define mu_inline static inline
#endif

// Definition of non-returning functions
#define mu_noreturn __attribute__((noreturn)) void

// Definition of pure functions
#define mu_pure __attribute__((const))

// Definition of unused variables
#define mu_unused __attribute__((unused))

// Definition of alignment for Mu types
#define mu_aligned(x) __attribute__((__aligned__(x)))

// Builtin for offset of structure members
#define mu_offsetof(t, m) __builtin_offsetof(t, m)

// Builtin for an unreachable point in code
#define mu_unreachable __builtin_unreachable()

// Builtin for the next power of two
#ifdef MU32
#define mu_npw2(x) (32 - __builtin_clz((x)-1))
#else
#define mu_npw2(x) (64 - __builtin_clzl((x)-1))
#endif

// Builtin for finding next alignment for pointers
#define mu_align(x) (((x) + sizeof(uintptr_t)-1) & ~(sizeof(uintptr_t)-1))

// Definition of Mu specific assert function
#ifdef MU_DEBUG
#include <assert.h>
#define mu_assert(x) assert(x)
#else
#define mu_assert(x)
#endif


#endif
