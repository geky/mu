#include "vparse.h"

#include "vlex.h"

#include <assert.h>



// Macro for accepting the current token and continuing
static inline struct vstate *vnext(struct vstate *vs) {
    vs->tok = vlex(vs);
    return vs;
}

// Parsing error handling
__attribute__((noreturn))
static void verror(struct vstate *vs) {
    printf("unexpected '%c' (%x)\n", vs->tok, vs->tok);
    assert(false); // TODO make this throw actual messages
}

// Expect a token or fail
static void vexpect(struct vstate *vs, vtok_t tok) {
    printf("expected '%c' (%x) not '%c' (%x)\n", tok, tok, vs->tok, vs->tok);
    assert(vlex(vs) == tok); // TODO
}



// A recursive descent parser implementation of
// V's grammar. Each nonterminal is represented 
// by one of the following functions.

static void vp_primary(struct vstate *vs) {
    switch (vs->tok) {
        case '(':       vp_primary(vnext(vs));
                        vexpect(vs, ')');
                        return;

        case VTOK_NUM:
        case VTOK_STR:  return;

        default:        verror(vs);
    }
}

static void vp_expression(struct vstate *vs) {
    switch (vs->tok) {
        case 0:         return;

        case '\n':
        case ',':
        case ';':       vp_expression(vnext(vs));
                        return;

        default:        vp_primary(vs);
                        vp_expression(vnext(vs));
                        return;
    }
}


// Parses V source code and evaluates the result
var_t vparse(struct vstate *vs) {
    vp_expression(vnext(vs));

    return vs->val;
}
