#include "parse.h"

#include "vm.h"
#include "lex.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include <stdio.h>
#include <string.h>


// Parsing error handling
static mu_noreturn void unexpected(parse_t *p) {
    mu_err_parse();
}

// Expect a token or fail
static void expect(parse_t *p, tok_t tok) {
    if (p->l.tok != tok)
        mu_err_parse();
    mu_lex(p);
}

static void emit(parse_t *p, data_t byte) {
    mstr_insert(&p->bcode, p->bcount, byte);
    p->bcount += 1;
}

static void encode(parse_t *p, op_t op,
                   uint_t d, uint_t a, uint_t b, len_t sdiff) {
    if (p->l.lookahead)
        return;

    p->sp += sdiff;

    if (p->sp > p->smax)
        p->smax = p->sp;

    mu_encode((void (*)(void *, data_t))emit, p, op, d, a, b);
}

static void encode_imm(parse_t *p, mu_t m) {
    static const ref_t imm_nil = 0;
    uint_t index = 0;

    if (!p->l.lookahead) {
        if (isnil(m))
            m = mtbl((tbl_t *)&imm_nil);

        mu_t mindex = tbl_lookup(p->imms, m);

        if (!isnil(mindex)) {
            index = getuint(mindex);
        } else {
            index = tbl_getlen(p->imms);
            tbl_assign(p->imms, m, muint(index));
        }
    }

    encode(p, OP_IMM, p->sp+1, index, 0, +1);
}

static void expect_indirect(parse_t *p) {
    if (!p->indirect)
        mu_err_parse();
}

static void expect_direct(parse_t *p) {
    if (p->indirect) {
        encode(p, OP_LOOKUP,
               p->sp - !p->scoped,
               p->scoped ? 0 : p->sp-1,
               p->sp,
               -(!p->scoped));
    }
}


static parse_t *parse_create(str_t *code) {
    parse_t *p = mu_alloc(sizeof(parse_t));

    memset(p, 0, sizeof(parse_t));

    p->bcode = mstr_create(0);
    p->bcount = 0;

    p->imms = tbl_create(0);
    p->fns = tbl_create(0);
    p->sp = 0;

    p->scoped = false;
    p->indirect = true;

    p->l.pos = str_getdata(code);
    p->l.end = str_getdata(code) + str_getlen(code);

    mu_lex(p);

    return p;
}

static code_t *parse_realize(parse_t *p) {
    struct code *code = ref_alloc(
        sizeof(struct code) +
        sizeof(mu_t)*tbl_getlen(p->imms) +
        sizeof(struct code *)*tbl_getlen(p->fns) +
        p->bcount);

    code->bcount = p->bcount;
    code->icount = tbl_getlen(p->imms);
    code->fcount = tbl_getlen(p->fns);

    code->flags.regs = p->smax;
    code->flags.scope = 4; // TODO
    code->flags.args = 0xf; // TODO
    code->flags.type = 0;

    mu_t *imms = code_imms(code);
    struct code **fns = code_fns(code);
    data_t *bcode = (data_t *)code_bcode(code);

    tbl_for_begin (k, v, p->imms) {
        if (istbl(k))
            imms[getuint(v)] = mnil;
        else
            imms[getuint(v)] = mu_inc(k);
    } tbl_for_end;

//  TODO
//    tbl_for_begin (k, v, p->fns) {
//        fns[getuint(k)] = mu_inc(v);
//    } tbl_for_end;

    tbl_dec(p->imms);
    tbl_dec(p->fns);

    memcpy(bcode, p->bcode->data, p->bcount);

    str_dec(p->bcode);
    mu_dealloc(p, sizeof(parse_t));

    return code;
}


// Grammar rules
static void p_expr(parse_t *p);
static void p_key(parse_t *p);
static void p_unpack_entry(parse_t *p);
static void p_pack_entry(parse_t *p);
static void p_list(parse_t *p);
static void p_pack(parse_t *p);
static void p_unpack(parse_t *p);
static void p_package(parse_t *p);
static void p_stmt(parse_t *p);
static void p_stmt_list(parse_t *p);


static void p_expr(parse_t *p) {
    switch (p->l.tok) {
        case T_SYM:
            encode_imm(p, p->l.val);
            mu_lex(p);
            p->indirect = true;
            p->scoped = true;
            break;

        case T_IMM:
            encode_imm(p, p->l.val);
            mu_lex(p);
            p->indirect = false;
            break;

        case '[':
            mu_lex(p);
            {   struct f_parse f = p->f;
                p->f.tabled = true;
                p_pack(p);
                p->f = f;
            }
            expect(p, ']');
            p->indirect = false;
            break;

        default:
            unexpected(p);
    }

    while (true) {
        switch (p->l.tok) {
            case T_KEY2:
                mu_lex(p);
                expect_direct(p);
                encode_imm(p, p->l.val);
                mu_lex(p);
                p->indirect = true;
                p->scoped = false;
                break;

            default:
                return;
        }
    }
}

static void p_key(parse_t *p) {
    switch (p->l.tok) {
        case T_KEY:
            mu_lex(p);
            encode_imm(p, p->l.val);
            p->indirect = false;
            break;

        default:
            p_expr(p);
            break;
    }
}

static void p_unpack_entry(parse_t *p) {
    if (p->f.tabled) {
        struct l_parse l = p->l;
        p->l.lookahead = true;
        p_key(p);

        if (p->l.tok == ':') {
            p->l = l;
            p_key(p);
            expect_direct(p);
            expect(p, ':');
        } else {
            p->l = l;
            encode_imm(p, muint(p->f.rcount));
        }

        encode(p, p->f.rcount == p->f.lcount-1 ? OP_LOOKUP : OP_ILOOKUP,
               p->sp, p->sp-1, p->sp, 0);
    }

    if (p->l.tok == '[') {
        mu_lex(p);
        struct f_parse f = p->f;
        p->f.tabled = true;
        p_unpack(p);
        p->f = f;
        expect(p, ']');
    } else {
        p_expr(p);
        expect_indirect(p);

        encode(p, p->insert ? OP_IINSERT : OP_IASSIGN,
               p->sp - !p->scoped -
                    (p->f.tabled ? 1 : (p->f.lcount-p->f.rcount)),
               p->scoped ? 0 : p->sp-1,
               p->sp,
               -(1 + !p->scoped + p->f.tabled));
    }
}

static void p_pack_entry(parse_t *p) {
    p_key(p);
    expect_direct(p);

    if (p->l.tok == ':') {
        mu_lex(p);
        p->f.tabled = true;
        p_expr(p);
        expect_direct(p);

        if (p->f.rcount >= p->f.lcount || !p->f.tabled) {
            encode(p, OP_DROP, p->sp, 0, 0, 0);
            encode(p, OP_DROP, p->sp-1, 0, 0, -2);
            p->f.rcount--;
        } else {
            encode(p, OP_IINSERT, p->sp, p->sp-2, p->sp-1, -2);
        }
    } else {
        if (p->f.rcount >= p->f.lcount) {
            encode(p, OP_DROP, p->sp, 0, 0, -1);
            p->f.rcount--;
        } else if (p->f.tabled) {
            encode_imm(p, muint(p->f.rcount));
            encode(p, OP_IINSERT, p->sp-1, p->sp-2, p->sp, -2);
        }
    }
}

static void p_list(parse_t *p) {
    p->f.rcount = 0;

    if (p->l.tok != T_KEY &&
        p->l.tok != T_SYM &&
        p->l.tok != T_IMM &&
        p->l.tok != '[') {
        return;
    }

    while (true) {
        if (p->l.lookahead || p->pack)
            p_pack_entry(p);
        else
            p_unpack_entry(p);

        p->f.rcount++;

        if (p->l.tok != ',')
            break;

        mu_lex(p);
    }
}

static void p_pack(parse_t *p) {
    struct l_parse l = p->l;
    p->l.lookahead = true;
    p->f.lcount = -1;
    p_list(p);
    p->f.lcount = p->f.rcount;
    p->f.tabled = p->f.tabled || p->f.lcount > MU_FRAME;
    p->l = l;

    if (p->f.tabled)
        encode(p, OP_TBL, p->sp+1, p->f.lcount, 0, +1);

    p->pack = true;
    p_list(p);
}

static void p_unpack(parse_t *p) {
    struct l_parse l = p->l;
    p->l.lookahead = true;
    p->f.lcount = -1;
    p_list(p);
    p->f.lcount = p->f.rcount;
    p->f.tabled = p->f.tabled || p->f.lcount > MU_FRAME;
    p->l = l;

    p->pack = false;
    p_list(p);
}

static void p_package(parse_t *p) {
    struct l_parse l = p->l;
    p->l.lookahead = true;
    p->f.lcount = -1;
    p_list(p);
    p->f.lcount = p->f.rcount;
    p->f.tabled = p->f.tabled || p->f.lcount > MU_FRAME;

    if (p->insert || p->l.tok == '=') {
        p->l.lookahead = l.lookahead;
        expect(p, '=');

        if (p->f.tabled)
            encode(p, OP_TBL, p->sp+1, p->f.lcount, 0, +1);

        p->pack = true;
        p_list(p);

        if (!p->f.tabled) {
            for (uint_t i = p->f.rcount; i < p->f.lcount; i++)
                encode_imm(p, mnil);
        }

        p->l = l;

        p->pack = false;
        p_list(p);
        expect(p, '=');

        p->l.lookahead = true;
        p->pack = true;
        p_list(p);
        p->l.lookahead = l.lookahead;
    } else {
        p->l = l;

        p->f.lcount = 0;
        p->pack = true;
        p_list(p);
    }
}

static void p_stmt(parse_t *p) {
    switch (p->l.tok) {
        case T_RETURN:
            mu_lex(p);
            p->f.tabled = false;
            p_pack(p);
            encode(p, OP_RET,
                      p->sp - (p->f.tabled ? 0 : p->f.lcount-1),
                      p->f.tabled ? 0xf : p->f.lcount, 0, 0);
            return;

        case T_LET:
            mu_lex(p);
            p->f.tabled = false;
            p->insert = true;
            return p_package(p);

        default:
            p->f.tabled = false;
            p->insert = false;
            return p_package(p);
    }
}

static void p_stmt_list(parse_t *p) {
    while (true) {
        p_stmt(p);

        if (p->l.tok != ';')
            break;

        mu_lex(p);
    }
}


code_t *mu_parse_expr(str_t *code) {
    parse_t *p = parse_create(code);
    // TODO find best entry point for expressions
    return parse_realize(p);
}

code_t *mu_parse_fn(str_t *code) {
    parse_t *p = parse_create(code);
    p_stmt_list(p);
    encode(p, OP_RET, 0, 0, 0, 0);
    return parse_realize(p);
}

code_t *mu_parse_module(str_t *code) {
    parse_t *p = parse_create(code);
    p_stmt_list(p);
    encode(p, OP_RET, 0, 0, 1, 0);
    return parse_realize(p);
}

