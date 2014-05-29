#include "vparse.h"

#include "vlex.h"
#include "tbl.h"

#include <assert.h>
#include <stdio.h>


// Macro for accepting the current token and continuing
static inline struct vstate *vnext(struct vstate *vs) {
    vs->tok = vlex(vs);
    return vs;
}

// Parsing error handling
__attribute__((noreturn))
static void verror(struct vstate *vs) {
    printf("unexpected '%c' (%02x)\n", vs->tok, vs->tok);
    assert(false); // TODO make this throw actual messages
}

// Expect a token or fail
static void vexpect(struct vstate *vs, vtok_t tok) {
    if (vlex(vs) != tok) {
        printf("expected '%c' (%02x) not '%c' (%x)\n", tok, tok, vs->tok, vs->tok);
        assert(vlex(vs) == tok); // TODO
    }
}



// A recursive descent parser implementation of
// V's grammar. Each nonterminal is represented 
// by one of the following functions.
static void vp_primep(struct vstate *vs);
static void vp_primsp(struct vstate *vs);
static void vp_primary(struct vstate *vs);
static void vp_explist(struct vstate *vs);
static void vp_expression(struct vstate *vs);

static void vp_primep(struct vstate *vs) {
    switch (vs->tok) {
        case '\n':      vp_primep(vnext(vs));
                        return;

        case ')':       return;

        default:        verror(vs);
    }
}

static void vp_primsp(struct vstate *vs) {
    switch (vs->tok) {
        case '\n':      vp_primsp(vnext(vs));
                        return;

        default:        vp_primary(vs);
                        vp_primep(vnext(vs));
                        return;
    }
}

static void vp_primary(struct vstate *vs) {
    switch (vs->tok) {
        case VTOK_IDENT:
                        vs->val = tblp_lookup(vs->scope, vs->val);
                        return;

        case VTOK_NUM:
        case VTOK_STR:  return;

        case '(':       vp_primsp(vnext(vs));
                        return;

        default:        verror(vs);
    }
}

static void vp_explist(struct vstate *vs) {
    switch(vs->tok) {
        case 0:         return;

        case '\n':
        case ',':
        case ';':       vp_expression(vnext(vs));
                        return;

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
                        vp_explist(vnext(vs));
                        return;
    }
}


// Parses V source code and evaluates the result
var_t vparse(struct vstate *vs) {
    vp_expression(vnext(vs));

    return vs->val;
}
