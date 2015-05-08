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
    if (p->l.pass)
        return;

    p->sp += sdiff;

    if (p->sp > p->smax)
        p->smax = p->sp;

    mu_encode((void (*)(void *, data_t))emit, p, op, d, a, b);
}

static uint_t imm(parse_t *p, mu_t m) {
    static const ref_t imm_nil = 0;
    
    if (p->l.pass)
        return 0;

    if (isnil(m))
        m = mtbl((tbl_t *)&imm_nil);

    mu_t mindex = tbl_lookup(p->imms, m);

    if (!isnil(mindex))
        return getuint(mindex);

    uint_t index = tbl_getlen(p->imms);
    tbl_assign(p->imms, m, muint(index));
    return index;
}
    

static void expect_indirect(parse_t *p) {
    if (p->state != P_INDIRECT && p->state != P_SCOPED)
        mu_err_parse();
}

static void expect_direct(parse_t *p) {
    if (p->state == P_INDIRECT)
        encode(p, OP_LOOKUP, p->sp-1, p->sp-1, p->sp, -1);
    else if (p->state == P_SCOPED)
        encode(p, OP_ILOOKUP, p->sp, 0, p->sp, 0);
    else if (p->state == P_CALLED)
        encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args), 
               p->args, 1, 
               1 - (p->args == 0xf ? 2 : p->args+1));
}

static void expect_second(parse_t *p) {
    if (p->state == P_INDIRECT) {
        encode(p, OP_LOOKUP, p->sp, p->sp-1, p->sp, 0);
    } else if (p->state == P_SCOPED) {
        encode(p, OP_ILOOKUP, p->sp+1, 0, p->sp, +1);
    } else {
        if (p->state == P_CALLED)
            encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args),
                   p->args, 1, 
                   1 - (p->args == 0xf ? 2 : p->args+1));

        encode(p, OP_DUP, p->sp+1, p->sp, 0, +1);
        encode(p, OP_DROP, p->sp-1, 0, 0, 0);
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

    p->op.lprec = -1;

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
static void p_postexpr(parse_t *p);
static void p_entry(parse_t *p);
static void p_frame(parse_t *p);
static void p_pack(parse_t *p);
static void p_unpack(parse_t *p);
static void p_stmt(parse_t *p);
static void p_stmt_list(parse_t *p);


static void p_expr(parse_t *p) {
    switch (p->l.tok) {
        case T_SYM:
            encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
            mu_lex(p);
            p->state = P_SCOPED;
            return p_postexpr(p);

        case T_IMM:
            encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
            mu_lex(p);
            p->state = P_DIRECT;
            return p_postexpr(p);

        case '[':
            mu_lex(p);
            {   struct f_parse f = p->f;
                p->f.tabled = true;
                p->f.unpack = false;
                p_pack(p);

                if (p->f.call)
                    encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args),
                           p->args, 1, 
                           1 - (p->args == 0xf ? 2 : p->args+1));

                p->f = f;
            }
            expect(p, ']');
            p->state = P_DIRECT;
            return p_postexpr(p);

        default:
            unexpected(p);
    }
}

static void p_postexpr(parse_t *p) {
    switch (p->l.tok) {
        case T_KEY2:
            mu_lex(p);
            expect_direct(p);
            encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
            mu_lex(p);
            p->state = P_INDIRECT;
            return p_postexpr(p);

        case T_REST:
        case T_OP:
            if (p->op.lprec <= p->op.rprec)
                return;

            {   len_t lprec = p->op.lprec;
                mu_t sym = mu_inc(p->l.val);
                p->op.lprec = p->op.rprec;
                mu_lex(p);
                expect_second(p);
                
                p_expr(p);
                expect_direct(p);

                encode(p, OP_IMM, p->sp-2, imm(p, sym), 0, 0);
                encode(p, OP_ILOOKUP, p->sp-2, 0, p->sp-2, 0);
                p->args = 2;
                p->op.lprec = lprec;
            }
            p->state = P_CALLED;
            return p_postexpr(p);

        case '(':
            mu_lex(p);
            expect_direct(p);
            {   struct f_parse f = p->f;
                p->f.tabled = false;
                p->f.unpack = false;
                p_pack(p);

                if (p->f.call)
                    encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args), 
                           p->args, p->f.tabled ? 0xf : p->f.lcount, 
                           (p->f.tabled ? 1 : p->f.lcount) 
                           - (p->args == 0xf ? 2 : p->args+1));

                p->args = p->f.tabled ? 0xf : p->f.lcount;
                p->f = f;
            }
            expect(p, ')');
            p->state = P_CALLED;
            return p_postexpr(p);

        default:
            return;
    }
}

static void p_entry(parse_t *p) {
    bool key = false;

    if (!p->l.pass && p->f.unpack && p->f.tabled) {
        struct l_parse l = p->l;
        p->l.pass = true;
        p_expr(p);

        if (p->l.tok == ':')
            key = true;
        else
            encode(p, OP_IMM, p->sp+1, imm(p, muint(p->f.rcount)), 0, +1);

        p->l = l;
    }

    if (p->l.pass || !p->f.unpack || key) {
        switch (p->l.tok) {
            case T_SYM:
                encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
                mu_lex(p);
                if (p->l.tok == ':') {
                    p->state = P_DIRECT;
                } else {
                    p->state = P_SCOPED;
                    p_postexpr(p);
                }
                break;

            default:
                p_expr(p);
                break;
        }

        if (p->l.tok == ':') {
            mu_lex(p);
            expect_direct(p);
            key = true;

            if (p->l.pass)
                p->f.tabled = true;
        }
    }

    if (!p->l.pass && p->f.unpack) {
        if (p->f.tabled)
            encode(p, p->f.rcount == p->f.lcount-1 ? OP_LOOKUP : OP_ILOOKUP,
                   p->sp+(p->f.rcount != p->f.lcount-1), p->sp-1, p->sp, 
                   +(p->f.rcount != p->f.lcount-1));

        if (p->l.tok == '[') {
            mu_lex(p);
            struct f_parse f = p->f;
            p->f.tabled = true;
            p->f.unpack = true;
            p_pack(p);
            p->f = f;
            expect(p, ']');
        } else {
            p_expr(p);
            expect_indirect(p);

            if (p->state == P_SCOPED)
                encode(p, p->insert ? OP_IINSERT : OP_IASSIGN,
                       p->sp - (p->f.tabled ? 1 : (p->f.lcount-p->f.rcount)),
                       0, p->sp, -(1 + p->f.tabled));
            else
                encode(p, p->insert ? OP_IINSERT : OP_IASSIGN,
                       p->sp-1 - (p->f.tabled ? 1 : (p->f.lcount-p->f.rcount)),
                       p->sp-1, p->sp, -(2 + p->f.tabled));
        }
    } else if (key) {
        expect_direct(p);
        p_expr(p);
        expect_direct(p);

        if (p->f.tabled) {
            encode(p, OP_IINSERT, p->sp, p->sp-2, p->sp-1, -2);
        } else {
            encode(p, OP_DROP, p->sp, 0, 0, -1);
            encode(p, OP_DROP, p->sp, 0, 0, -1);
            p->f.rcount--;
        }
    } else if (p->l.tok != ',' && p->f.rcount == 0 && p->state == P_CALLED) {
        p->f.call = true;
    } else {
        expect_direct(p);

        if (p->f.tabled) {
            encode(p, OP_IMM, p->sp+1, imm(p, muint(p->f.rcount)), 0, +1);
            encode(p, OP_IINSERT, p->sp-1, p->sp-2, p->sp, -2);
        } else if (p->f.rcount >= p->f.lcount) {
            encode(p, OP_DROP, p->sp, 0, 0, -1);
            p->f.rcount--;
        }
    }
}

static void p_frame(parse_t *p) {
    p->f.rcount = 0;
    p->f.call = false;

    if (p->l.tok != T_SYM &&
        p->l.tok != T_IMM &&
        p->l.tok != '[') {
        return;
    }

    if (!p->l.pass && !p->f.unpack && p->f.tabled)
        encode(p, OP_TBL, p->sp+1, p->f.lcount, 0, +1);

    while (true) {
        p_entry(p);
        p->f.rcount++;

        if (p->l.tok != ',')
            break;

        mu_lex(p);
    }

    if (p->l.pass && p->f.rcount > MU_FRAME)
        p->f.tabled = true;

    if (!p->l.pass && !p->f.unpack && !p->f.tabled && !p->f.call) {
        for (uint_t i = p->f.rcount; i < p->f.lcount; i++)
            encode(p, OP_IMM, p->sp+1, imm(p, mnil), 0, +1);
    }
}

static void p_pack(parse_t *p) {
    struct l_parse l = p->l;
    p->l.pass = true;
    p->f.lcount = -1;
    p_frame(p);
    p->f.lcount = p->f.rcount;
    p->l = l;

    p_frame(p);
}

static void p_unpack(parse_t *p) {
    p->f.tabled = false;

    struct l_parse l = p->l;
    p->l.pass = true;
    p->f.lcount = -1;
    p_frame(p);
    p->f.lcount = p->f.rcount;

    if (p->insert || p->l.tok == '=') {
        expect(p, '=');

        p->l.pass = l.pass;
        p->f.unpack = false;
        p_frame(p);

        if (p->f.call)
            encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args),
                   p->args, 1, 
                   1 - (p->args == 0xf ? 2 : p->args+1));

        p->l = l;
        p->f.unpack = true;
        p_frame(p);

        expect(p, '=');

        p->l.pass = true;
        p_frame(p);
        p->l.pass = l.pass;
    } else {
        p->l = l;
        p->f.lcount = 0;
        p->f.unpack = false;
        p_frame(p);

        if (p->f.call)
            encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args),
                   p->args, 0,
                   0 - (p->args == 0xf ? 2 : p->args+1));
    }

    if (!p->f.tabled)
        p->sp -= p->f.lcount;
}

static void p_stmt(parse_t *p) {
    switch (p->l.tok) {
        case T_ARROW:
        case T_RETURN:
            mu_lex(p);
            p->f.tabled = false;
            p->f.unpack = false;
            p_pack(p);

            if (p->f.call)
                encode(p, OP_TCALL, 
                       p->sp - (p->args == 0xf ? 1 : p->args), 
                       p->args, 0, 
                       -(p->args == 0xf ? 2 : p->args+1));
            else
                encode(p, OP_RET,
                       p->sp - (p->f.tabled ? 0 : p->f.lcount-1),
                       0, p->f.tabled ? 0xf : p->f.lcount, 
                       -(p->f.tabled ? 1 : p->f.lcount));
            return;

        case T_LET:
            mu_lex(p);
            p->f.tabled = false;
            p->insert = true;
            return p_unpack(p);

        default:
            p->f.tabled = false;
            p->insert = false;
            return p_unpack(p);
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

