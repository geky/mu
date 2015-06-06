#include "parse.h"

#include "vm.h"
#include "lex.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include <stdio.h>
#include <string.h>


// TODO make all these expects better messages
// Parsing error handling
static mu_noreturn void unexpected(parse_t *p) {
    mu_err_parse();
}

// Expect a token or fail
static void expect(parse_t *p, tok_t tok) {
    if (p->l.tok != tok)
        mu_err_parse();
    mu_lex(&p->l);
}

static void emit(parse_t *p, data_t byte) {
    mstr_insert(&p->bcode, p->bcount, byte);
    p->bcount += 1;
}

static void encode(parse_t *p, op_t op,
                   uint_t d, uint_t a, uint_t b, 
                   len_t sdiff) {
    if (p->l.lookahead)
        return;

    p->sp += sdiff;

    if (p->sp+1 > p->flags.regs)
        p->flags.regs = p->sp+1;

    mu_encode((void (*)(void *, data_t))emit, p, op, d, a, b);
}

static void patch(parse_t *p, len_t offset, op_t op,
                  uint_t d, uint_t a) {
    if (p->l.lookahead)
        return;

    len_t count = p->bcount;
    p->bcount = offset;
    mu_encode((void (*)(void *, data_t))emit, p, op, d, a, 0);
    p->bcount = count;
}

static uint_t imm(parse_t *p, mu_t m) {
    static const ref_t imm_nil = 0;

    if (p->l.lookahead)
        return 0;
    
    if (isnil(m))
        m = mtbl((tbl_t *)&imm_nil);

    mu_t mindex = tbl_lookup(p->imms, m);

    if (!isnil(mindex))
        return getuint(mindex);

    uint_t index = tbl_getlen(p->imms);
    tbl_insert(p->imms, m, muint(index));
    return index;
}

static uint_t fn(parse_t *p, code_t *code) {
    uint_t index = tbl_getlen(p->fns);
    tbl_insert(p->fns, muint(index), mfn(fn_create(code, 0)));
    return index;
}   

static void expect_indirect(parse_t *p) {
    if (p->state != P_INDIRECT && p->state != P_SCOPED)
        mu_err_parse();
}

static void expect_direct(parse_t *p) {
    if (p->state == P_INDIRECT)
        encode(p, OP_LOOKDN, p->sp-1, p->sp-1, p->sp, -1);
    else if (p->state == P_SCOPED)
        encode(p, OP_LOOKUP, p->sp, 0, p->sp, 0);
    else if (p->state == P_CALLED)
        encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args), 
               p->args, 1, 
               1 - (p->args == 0xf ? 2 : p->args+1));
}

static void expect_second(parse_t *p) {
    if (p->state == P_INDIRECT) {
        encode(p, OP_LOOKDN, p->sp, p->sp-1, p->sp, 0);
    } else if (p->state == P_SCOPED) {
        encode(p, OP_LOOKUP, p->sp+1, 0, p->sp, +1);
    } else {
        if (p->state == P_CALLED)
            encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args),
                   p->args, 1, 
                   1 - (p->args == 0xf ? 2 : p->args+1));

        encode(p, OP_MOVE, p->sp+1, p->sp, 0, +1);
    }
}


static parse_t *parse_create(str_t *code) {
    parse_t *p = mu_alloc(sizeof(parse_t));

    memset(p, 0, sizeof(parse_t));

    p->bcode = mstr_create(0);
    p->bcount = 0;

    p->imms = tbl_create(0);
    p->fns = tbl_create(0);
    p->flags.regs = 1;
    p->flags.scope = 4; // TODO
    p->flags.args = 0;
    p->flags.type = 0;

    p->l.pos = str_getdata(code);
    p->l.end = str_getdata(code) + str_getlen(code);
    mu_lex(&p->l);

    return p;
}

static parse_t *parse_nested(parse_t *p) {
    parse_t *q = mu_alloc(sizeof(parse_t));

    memset(q, 0, sizeof(parse_t));

    q->bcode = mstr_create(0);
    q->bcount = 0;

    q->imms = tbl_create(0);
    q->fns = tbl_create(0);
    q->flags.regs = 1;
    q->flags.scope = 4; // TODO
    q->flags.args = 0;
    q->flags.type = 0;

    q->l = p->l;

    return q;
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
    code->flags = p->flags;

    mu_t *imms = code_imms(code);
    struct code **fns = code_fns(code);
    data_t *bcode = (data_t *)code_bcode(code);

    tbl_for_begin (k, v, p->imms) {
        if (istbl(k))
            imms[getuint(v)] = mnil;
        else
            imms[getuint(v)] = mu_inc(k);
    } tbl_for_end;

    tbl_for_begin (k, v, p->fns) {
        fns[getuint(k)] = getfn(mu_inc(v))->code;
    } tbl_for_end;

    tbl_dec(p->imms);
    tbl_dec(p->fns);

    memcpy(bcode, p->bcode->data, p->bcount);

    str_dec(p->bcode);
    mu_dealloc(p, sizeof(parse_t));

    return code;
}


// Grammar rules
static void p_fn(parse_t *p);
static void p_if(parse_t *p);
static void p_expr(parse_t *p);
static void p_postexpr(parse_t *p);
static void p_item(parse_t *p);
static void p_entry(parse_t *p);
static void p_frame(parse_t *p);
static void p_pack(parse_t *p);
static void p_unpack(parse_t *p);
static void p_stmt(parse_t *p);
static void p_multistmt(parse_t *p);


static void p_fn(parse_t *p) {
    if (p->l.lookahead) {
        struct f_parse f = p->f;
        p->l.paren++;
        expect(p, '(');
        p_frame(p);
        p->l.paren--;
        expect(p, ')');

        uintq_t single = p->single;
        p->single = true;
        p_stmt(p);
        p->single = single;
        p->f = f;
    } else {
        parse_t *q = parse_nested(p);

        p->l.paren++;
        expect(q, '(');
        struct f_parse f = q->f;
        q->f.unpack = true;
        q->insert = true;

        lex_t l = q->l;
        q->l.lookahead = true;
        p_frame(q);
        q->l = l;

        q->flags.args = q->f.tabled ? 0xf : q->f.lcount;
        q->sp += q->f.tabled ? 1 : q->f.lcount;

        p_frame(q);

        q->sp -= q->f.tabled ? 1 : q->f.lcount;
        q->f = f;
        p->l.paren--;
        expect(q, ')');

        q->single = true;
        p_stmt(q);
        p->l = q->l;

        encode(p, OP_FN, p->sp+1, fn(p, parse_realize(q)), 0, +1);
    }
}

static void p_if(parse_t *p) {
    p->l.paren++;
    expect(p, '(');
    p_item(p);
    p->l.paren--;
    expect(p, ')');

    len_t before_if = p->bcount;
    encode(p, OP_JFALSE, p->sp, 0, 0, 0);
    len_t after_if = p->bcount;
    encode(p, OP_DROP, p->sp, 0, 0, -1);
    p_item(p);

    if (p->l.tok == T_ELSE) {
        mu_lex(&p->l);
        len_t before_else = p->bcount;
        encode(p, OP_JUMP, 0, 0, 0, -1);
        len_t after_else = p->bcount;
        p_item(p);

        patch(p, before_if, OP_JFALSE, p->sp, after_else - after_if);
        patch(p, before_else, OP_JUMP, 0, p->bcount - after_else);
    } else {
        len_t before_else = p->bcount;
        encode(p, OP_JUMP, 0, 0, 0, -1);
        len_t after_else = p->bcount;
        encode(p, OP_IMM, p->sp+1, imm(p, mnil), 0, +1);

        patch(p, before_if, OP_JFALSE, p->sp, after_else - after_if);
        patch(p, before_else, OP_JUMP, 0, p->bcount - after_else);
    }
}   

static void p_expr(parse_t *p) {
    switch (p->l.tok) {
        case '(':
            p->l.paren++;
            mu_lex(&p->l);
            {   uintq_t lprec = p->l.lprec;
                p->l.lprec = MU_MAXUINTQ;
                p_expr(p);
                p->l.lprec = lprec;
            }
            p->l.paren--;
            expect(p, ')');
            return p_postexpr(p);

        case '[':
            p->l.paren++;
            mu_lex(&p->l);
            {   struct f_parse f = p->f;
                p->f.tabled = true;
                p->f.unpack = false;
                p_pack(p);

                if (p->f.call)
                    encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args),
                           p->args, 0xf,
                           1 - (p->args == 0xf ? 2 : p->args+1));

                p->f = f;
            }
            p->l.paren--;
            expect(p, ']');
            p->state = P_DIRECT;
            return p_postexpr(p);

        case T_OP:
            encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
            encode(p, OP_LOOKUP, p->sp, 0, p->sp, 0);
            mu_lex(&p->l);
            p_expr(p);
            expect_direct(p);
            p->args = 1;
            p->state = P_CALLED;
            return p_postexpr(p);

        case T_SYM:
            encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
            mu_lex(&p->l);
            p->state = P_SCOPED;
            return p_postexpr(p);

        case T_IMM:
            encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
            mu_lex(&p->l);
            p->state = P_DIRECT;
            return p_postexpr(p);

        case T_FN:
            mu_lex(&p->l);
            p_fn(p);
            p->state = P_DIRECT;
            return p_postexpr(p);

        case T_IF:
            mu_lex(&p->l);
            p_if(p);
            p->state = P_DIRECT;
            return p_postexpr(p);

        default:
            unexpected(p);
    }
}

static void p_postexpr(parse_t *p) {
    switch (p->l.tok) {
        case '(':
            p->l.paren++;
            mu_lex(&p->l);
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
            p->l.paren--;
            expect(p, ')');
            p->state = P_CALLED;
            return p_postexpr(p);

        case T_REST:
        case T_OP:
            if (p->l.lprec <= p->l.rprec)
                return;

            {   uintq_t lprec = p->l.lprec;
                p->l.lprec = p->l.rprec;
                mu_t sym = mu_inc(p->l.val);
                mu_lex(&p->l);
                expect_second(p);
                
                p_expr(p);
                expect_direct(p);

                encode(p, OP_IMM, p->sp-2, imm(p, sym), 0, 0);
                encode(p, OP_LOOKUP, p->sp-2, 0, p->sp-2, 0);
                p->l.lprec = lprec;
            }
            p->args = 2;
            p->state = P_CALLED;
            return p_postexpr(p);

        case T_DOT:
            mu_lex(&p->l);
            expect_direct(p);
            if (p->l.tok == T_SYM)
                encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
            expect(p, T_SYM);
            p->state = P_INDIRECT;
            return p_postexpr(p);

        default:
            return;
    }
}

static void p_item(parse_t *p) {
    uintq_t lprec = p->l.lprec;
    p->l.lprec = MU_MAXUINTQ;
    p_expr(p);
    p->l.lprec = lprec;

    expect_direct(p);
}

static void p_entry(parse_t *p) {
    p->l.lprec = MU_MAXUINTQ;

    if (p->l.lookahead) {
        p_expr(p);

        if (p->l.tok == ':') {
            mu_lex(&p->l);
            p->f.tabled = true;

            p_expr(p);
        } else if (p->l.tok != ',' && 
                   p->f.lcount == 0 && 
                   p->state == P_CALLED) {
            p->f.call = true;
        }
    } else {
        bool key = false;

        if (p->f.unpack && p->f.tabled) {
            lex_t l = p->l;
            p->l.lookahead = true;
            p_expr(p);
            p->l.lookahead = false;

            if (p->l.tok == ':')
                key = true;
            else
                encode(p, OP_IMM, p->sp+1, imm(p, muint(p->f.rcount)), 0, +1);

            p->l = l;
        }

        if (!p->f.unpack || key) {
            switch (p->l.tok) {
                case T_SYM:
                    encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
                    mu_lex(&p->l);
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
                mu_lex(&p->l);
                expect_direct(p);
                key = true;
            }
        }

        if (p->f.unpack) {
            if (p->f.tabled)
                encode(p, p->f.rcount == p->f.lcount-1 ? OP_LOOKDN : OP_LOOKUP,
                       p->sp-(p->f.rcount == p->f.lcount-1), p->sp-1, p->sp, 
                       -(p->f.rcount == p->f.lcount-1));

            if (p->l.tok == '[') {
                mu_lex(&p->l);
                struct f_parse f = p->f;
                p->f.tabled = true;
                p->f.unpack = true;
                p_pack(p);
                p->f = f;
                expect(p, ']');
            } else {
                p_expr(p);
                expect_indirect(p);

                if (p->state == P_SCOPED) {
                    encode(p, p->insert ? OP_INSERT : OP_ASSIGN,
                           p->sp - (p->f.tabled ? 1 : 
                                    (p->f.lcount - p->f.rcount)),
                           0, p->sp, 
                           -(1 + (p->f.tabled && 
                                    p->f.rcount != p->f.lcount-1)));
                } else {
                    encode(p, p->insert ? OP_INSERT : OP_ASSIGN,
                           p->sp-1 - (p->f.tabled ? 1 : 
                                    (p->f.lcount - p->f.rcount)),
                           p->sp-1, p->sp, 0);

                    encode(p, OP_DROP, p->sp-1, 0, 0, 
                           -(2 + (p->f.tabled && 
                                    p->f.rcount != p->f.lcount-1)));
                }
            }
        } else if (key) {
            expect_direct(p);
            p_expr(p);
            expect_direct(p);

            if (p->f.tabled) {
                encode(p, OP_INSERT, p->sp, p->sp-2, p->sp-1, -2);
            } else {
                encode(p, OP_DROP, p->sp, 0, 0, -1);
                encode(p, OP_DROP, p->sp, 0, 0, -1);
                p->f.rcount--;
            }
        } else if (!p->f.call) {
            expect_direct(p);

            if (p->f.tabled) {
                encode(p, OP_IMM, p->sp+1, imm(p, muint(p->f.rcount)), 0, +1);
                encode(p, OP_INSERT, p->sp-1, p->sp-2, p->sp, -2);
            } else if (p->f.rcount >= p->f.lcount) {
                encode(p, OP_DROP, p->sp, 0, 0, -1);
                p->f.rcount--;
            }
        }
    }
}

static void p_frame(parse_t *p) {
    if (p->l.lookahead) {
        p->f.lcount = 0;
        p->f.call = false;
    } else {
        p->f.rcount = 0;
    }

    switch (p->l.tok) {
        case T_SYM: // TODO get rid of this nonsense
        case T_IMM:
        case '(':
        case '[':
        case T_FN:
        case T_IF:
        case T_OP:
            while (true) {
                p_entry(p);

                if (p->l.lookahead)
                    p->f.lcount++;
                else
                    p->f.rcount++;

                if (p->single || p->l.tok != ',')
                    break;

                mu_lex(&p->l);
            }
            break;

        default:
            return;
    }

    if (p->l.lookahead && p->f.lcount > MU_FRAME)
        p->f.tabled = true;
}

static void p_pack(parse_t *p) {
    if (p->l.lookahead) {
        p_frame(p);
    } else {
        lex_t l = p->l;
        p->l.lookahead = true;
        p_frame(p);
        p->l = l;

        if (p->f.tabled && !p->f.unpack && !p->f.call)
            encode(p, OP_TBL, p->sp+1, p->f.lcount, 0, +1);

        p_frame(p);
    }
}

static void p_unpack(parse_t *p) {
    if (p->l.lookahead) {
        p_frame(p);

        if (p->l.tok == '=') {
            mu_lex(&p->l);
            p_frame(p);
        }
    } else {
        p->f.tabled = false;

        lex_t l = p->l;
        p->l.lookahead = true;
        p_frame(p);
        p->l.lookahead = false;

        if (p->insert || p->l.tok == '=') {
            expect(p, '=');

            if (p->f.tabled) {
                lex_t l = p->l;
                p->l.lookahead = true;
                p_frame(p);
                p->l = l;

                if (!p->f.call)
                    encode(p, OP_TBL, p->sp+1, p->f.rcount, 0, +1);
            }

            p->f.unpack = false;
            p_frame(p);

            if (p->f.rcount == 0 || p->f.lcount == 0) {
                mu_err_parse();
            } else if (p->f.call) {
                encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args),
                       p->args, p->f.tabled ? 0xf : p->f.lcount,
                       (p->f.tabled ? 1 : p->f.lcount)
                       - (p->args == 0xf ? 2 : p->args+1));
            } else if (!p->f.tabled) {
                for (uint_t i = p->f.rcount; i < p->f.lcount; i++)
                    encode(p, OP_IMM, p->sp+1, imm(p, mnil), 0, +1);
            }

            p->l = l;
            p->f.unpack = true;
            p_frame(p);

            expect(p, '=');
            p->l.lookahead = true;
            p_frame(p);
            p->l.lookahead = false;

            if (!p->f.tabled)
                p->sp -= p->f.rcount;
        } else {
            p->l = l;
            p->f.lcount = 0;
            p->f.unpack = false;
            p_frame(p);

            if (p->f.call)
                encode(p, OP_CALL, p->sp-(p->args == 0xf ? 1 : p->args),
                       p->args, 0,
                       0 - (p->args == 0xf ? 2 : p->args+1));
            else
                p->sp -= p->f.tabled ? 1 : p->f.lcount;
        }
    }
}

static void p_stmt(parse_t *p) {
    switch (p->l.tok) {
        case '{':
            return p_multistmt(p);

        case T_ARROW:
        case T_RETURN:
            mu_lex(&p->l);
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
            mu_lex(&p->l);
            p->f.tabled = false;
            p->insert = true;
            return p_unpack(p);

        default:
            p->f.tabled = false;
            p->insert = false;
            return p_unpack(p);
    }
}

static void p_multistmt(parse_t *p) {
    uintq_t indent = 0;
    while (p->l.tok == '{') {
        indent += getuint(p->l.val);
        mu_lex(&p->l);
    }

    while (true) {
        uintq_t paren = p->l.paren;
        p->l.paren = false;
        uintq_t single = p->single;
        p->single = false;

        while (true) {
            p_stmt(p);

            if (p->l.tok != ';')
                break;

            mu_lex(&p->l);
        }

        p->single = single;
        p->l.paren = paren;

        if (indent == 0) {
            break;
        } else if (p->l.tok != '}') {
            mu_err_parse(); // TODO better message blablabla
        } else if (indent < getuint(p->l.val)) {
            p->l.val = muint(getuint(p->l.val) - indent);
            break;
        } else if (indent == getuint(p->l.val)) {
            mu_lex(&p->l);
            break;
        } else {
            indent -= getuint(p->l.val);
            mu_lex(&p->l);
        }
    }
}

code_t *mu_parse_expr(str_t *code) {
    parse_t *p = parse_create(code);
    // TODO find best entry point for expressions
    return parse_realize(p);
}

code_t *mu_parse_fn(str_t *code) {
    parse_t *p = parse_create(code);
    p_multistmt(p);
    encode(p, OP_RET, 0, 0, 0, 0);
    return parse_realize(p);
}

code_t *mu_parse_module(str_t *code) {
    parse_t *p = parse_create(code);
    p_multistmt(p);
    encode(p, OP_RET, 0, 0, 1, 0);
    return parse_realize(p);
}

