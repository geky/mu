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
static void vp_primary(struct vstate *vs);
static void vp_explist(struct vstate *vs);
static void vp_expression(struct vstate *vs);


static void vp_primary(struct vstate *vs) {
    switch (vs->tok) {
        case VT_IDENT:  vs->val = tblp_lookup(vs->scope, vs->val);
                        return;

        case VT_NUM:
        case VT_STR:    return;

        case '(':       vs->paren++;
                        vp_primary(vnext(vs));
                        vexpect(vs, ')');
                        vs->paren--;
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
    vs->paren = 0;
    vs->tok = vlex(vs);

    vp_expression(vs);

    return vs->val;
}
