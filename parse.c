#include "parse.h"

#include "vm.h"
#include "lex.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "err.h"
#include <stdio.h>
#include <string.h>


static void emit(struct parse *p, byte_t byte) {
    uint_t bcount = p->bcount;
    mstr_insert(&p->bcode, &bcount, byte);
    p->bcount = bcount;
}

static void encode(struct parse *p, op_t op,
                   uint_t d, uint_t a, uint_t b,
                   len_t sdiff) {
    p->sp += sdiff;

    if (p->sp+1 > p->regs)
        p->regs = p->sp+1;

    mu_encode((void (*)(void *, byte_t))emit, p, op, d, a, b);
}

static void patch(struct parse *p, len_t offset, op_t op,
                  uint_t d, uint_t a) {
    len_t count = p->bcount;
    p->bcount = offset;
    mu_encode((void (*)(void *, byte_t))emit, p, op, d, a, 0);
    p->bcount = count;
}

static mu_const mu_t imm_nil(void) {
    static mu_t nil = 0;
    if (nil) return nil;

    nil = mbfn(0, 0);
    return nil;
}

static uint_t imm(struct parse *p, mu_t m) {
    if (!m)
        m = imm_nil();

    mu_t mindex = tbl_lookup(p->imms, m);

    if (mindex)
        return num_uint(mindex);

    uint_t index = tbl_len(p->imms);
    tbl_insert(p->imms, m, muint(index));
    return index;
}

static uint_t fn(struct parse *p, struct code *code) {
    uint_t index = tbl_len(p->fns);
    tbl_insert(p->fns, muint(index), mfn(code, 0));
    return index;
}


// TODO make all these expects better messages
// Parsing error handling
static mu_noreturn unexpected(struct parse *p) {
    mu_err_parse();
}

// Expect a token or fail
static void expect(struct parse *p, enum tok tok) {
    if (p->l.tok != tok)
        mu_err_parse();
    mu_lex(&p->l);
}

static void expect_indirect(struct parse *p) {
    if (p->state != P_INDIRECT &&
        p->state != P_SCOPED &&
        p->state != P_NIL)
        mu_err_parse();
}

static void expect_direct(struct parse *p, uint_t offset) {
    if (p->state == P_INDIRECT) {
        encode(p, OP_LOOKDN, p->sp+offset-1, p->sp-1, p->sp, +offset-1);
    } else if (p->state == P_SCOPED) {
        encode(p, OP_LOOKUP, p->sp+offset, 0, p->sp, +offset);
    } else if (p->state == P_NIL) {
        encode(p, OP_IMM, p->sp+offset+1, imm(p, mnil), 0, +offset+1);
    } else {
        if (p->state == P_CALLED)
            encode(p, OP_CALL, p->sp-(p->params == 0xf ? 1 : p->params),
                   p->params, 1,
                   -(p->params == 0xf ? 1 : p->params));

        if (offset != 0)
            encode(p, OP_MOVE, p->sp+offset, p->sp, 0, +offset);
    }
}


static struct parse *parse_create(mu_t source) {
    struct parse *p = mu_alloc(sizeof(struct parse));

    memset(p, 0, sizeof(struct parse));

    p->bcode = mstr_create(0);
    p->bcount = 0;

    p->imms = tbl_create(0);
    p->fns = tbl_create(0);
    p->regs = 1;
    p->scope = 4; // TODO
    p->args = 0;

    p->l.pos = str_bytes(source);
    p->l.end = str_bytes(source) + str_len(source);
    mu_lex_init(&p->l);

    return p;
}

static struct parse *parse_nested(struct parse *p) {
    struct parse *q = mu_alloc(sizeof(struct parse));

    memset(q, 0, sizeof(struct parse));

    q->bcode = mstr_create(0);
    q->bcount = 0;

    q->imms = tbl_create(0);
    q->fns = tbl_create(0);
    q->regs = 1;
    q->scope = 4; // TODO
    q->args = 0;

    q->f = p->f;
    q->l = p->l;

    return q;
}

static struct code *parse_realize(struct parse *p) {
    struct code *code = ref_alloc(
        sizeof(struct code) +
        sizeof(mu_t)*tbl_len(p->imms) +
        sizeof(struct code *)*tbl_len(p->fns) +
        p->bcount);

    code->args = p->args;
    code->regs = p->regs;
    code->scope = p->scope;
    code->icount = tbl_len(p->imms);
    code->fcount = tbl_len(p->fns);
    code->bcount = p->bcount;

    mu_t *imms = code_imms(code);
    struct code **fns = code_fns(code);
    byte_t *bcode = (byte_t *)code_bcode(code);

    mu_t k, v;
    for (uint_t i = 0; tbl_next(p->imms, &i, &k, &v);) {
        if (k == imm_nil())
            imms[num_uint(v)] = mnil;
        else
            imms[num_uint(v)] = mu_inc(k);
    }

    for (uint_t i = 0; tbl_next(p->fns, &i, &k, &v);) {
        fns[num_uint(k)] = fn_code(v);
    }

    tbl_dec(p->imms);
    tbl_dec(p->fns);

    memcpy(bcode, p->bcode, p->bcount);

    mstr_dec(p->bcode);
    mu_dealloc(p, sizeof(struct parse));

    return code;
}


// Scanning rules
static void p_block_scan(struct parse *p);
static void p_expr_scan(struct parse *p);
static void p_postexpr_scan(struct parse *p);
static void p_frame_scan(struct parse *p);

static void p_block_scan(struct parse *p) {
    if (p->l.tok == T_LBLOCK) {
        uintq_t block = p->l.block;
        mu_scan(&p->l);
        while (p->l.block > block && p->l.tok) {
            mu_scan(&p->l);
        }
    } else {
        while (mu_isstmt(p->l.tok) ||
               (p->l.paren > p->f.paren && p->l.tok == T_SEP)) {
            mu_scan(&p->l);
        }
    }
}

static void p_expr_scan(struct parse *p) {
    while (p->l.tok == T_LPAREN)
        mu_scan(&p->l);

    return p_postexpr_scan(p);
}

static void p_postexpr_scan(struct parse *p) {
    switch (p->l.tok) {
        case T_LTABLE:
            {   uintq_t paren = p->l.paren;
                mu_scan(&p->l);
                while (p->l.paren > paren && p->l.tok)
                    mu_scan(&p->l);
            }
            p->f.call = false;
            return p_postexpr_scan(p);

        case T_LPAREN:
            {   uintq_t paren = p->l.paren;
                mu_scan(&p->l);
                while (p->l.paren > paren && p->l.tok)
                    mu_scan(&p->l);
            }
            p->f.call = true;
            return p_postexpr_scan(p);

        case T_OP:
        case T_EXPAND:
            mu_scan(&p->l);
            p_expr_scan(p);
            p->f.call = true;
            return p_postexpr_scan(p);

        case T_AND:
        case T_OR:
            // TODO handle these differently to prevent issues
            // with things like this?
            // let a = b or c  +  2
        case T_FN:
        case T_TYPE:
        case T_IF:
        case T_WHILE:
        case T_FOR:
        case T_ELSE:
            mu_scan(&p->l);
            p_postexpr_scan(p);
            p_block_scan(p);
            p->f.call = false;
            return p_postexpr_scan(p);

        case T_NIL:
        case T_IMM:
        case T_SYM:
        case T_KEY:
        case T_DOT:
        case T_ARROW:
            mu_scan(&p->l);
            p->f.call = false;
            return p_postexpr_scan(p);

        case T_RPAREN:
            if (p->f.count == 0 && p->l.paren > p->f.paren) {
                mu_scan(&p->l);
                return p_postexpr_scan(p);
            }
            return;

        default:
            return;
    }
}

static void p_frame_scan(struct parse *p) {
    p->f.single = p->l.paren == p->f.paren;
    uintq_t paren = p->f.paren;
    p->f.paren = p->l.paren;
    p->f.tabled = false;
    p->f.expand = false;
    p->f.call = false;
    p->f.count = 0;

    while (mu_isexpr(p->l.tok)) {
        if (p->l.tok == T_EXPAND) {
            mu_lex(&p->l);
            p->f.expand = true;
            p_expr_scan(p);
            break;
        }

        p_expr_scan(p);
        if (p->l.tok == T_PAIR) {
            mu_scan(&p->l);
            p->f.tabled = true;
            p_expr_scan(p);
        }

        p->f.count++;

        if (p->f.single || p->l.tok != T_SEP)
            break;

        p->f.call = false;
        mu_scan(&p->l);
    }

    p->f.tabled = p->f.tabled || p->f.expand || p->f.count > MU_FRAME;
    p->f.call = p->f.call && p->f.count == 1 && !p->f.tabled;
    p->f.target = p->f.count;
    p->f.paren = paren;
}


// Grammar rules
static void p_fn(struct parse *p);
static void p_if(struct parse *p);
static void p_subexpr(struct parse *p);
static void p_postexpr(struct parse *p);
static void p_expr(struct parse *p);
static void p_entry(struct parse *p);
static void p_frame(struct parse *p);
static void p_assignment(struct parse *p);
static void p_stmt(struct parse *p);
static void p_block(struct parse *p);

static void p_fn(struct parse *p) {
    struct parse *q = parse_nested(p);

    expect(q, T_LPAREN);
    {   struct f_parse f = q->f;
        struct lex l = q->l;
        p_frame_scan(q);
        q->l = l;

        q->sp += q->f.tabled ? 1 : q->f.count;
        q->args = q->f.tabled ? 0xf : q->f.count;

        q->f.unpack = true;
        q->f.insert = true;
        p_frame(q);
        q->f = f;
    }
    expect(q, T_RPAREN);

    p_stmt(q);
    encode(q, OP_RET, 0, 0, 0, 0);

    p->l = q->l;
    encode(p, OP_FN, p->sp+1, fn(p, parse_realize(q)), 0, +1);
}

static void p_if(struct parse *p) {
    expect(p, T_LPAREN);
    p_expr(p);
    expect(p, T_RPAREN);
    expect_direct(p, 0);

    len_t before_if = p->bcount;
    encode(p, OP_JFALSE, p->sp, 0, 0, 0);
    len_t after_if = p->bcount;
    encode(p, OP_DROP, p->sp, 0, 0, -1);
    p_expr(p);
    expect_direct(p, 0);

    if (p->l.tok == T_ELSE) {
        mu_lex(&p->l);
        len_t before_else = p->bcount;
        encode(p, OP_JUMP, 0, 0, 0, -1);
        len_t after_else = p->bcount;
        p_expr(p);
        expect_direct(p, 0);

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

static void p_subexpr(struct parse *p) {
    switch (p->l.tok) {
        case T_LPAREN:
            mu_lex(&p->l);
            {   uintq_t lprec = p->l.lprec;
                p->l.lprec = MU_MAXUINTQ;
                p_subexpr(p);
                p->l.lprec = lprec;
            }
            expect(p, T_RPAREN);
            return p_postexpr(p);

        case T_LTABLE:
            mu_lex(&p->l);
            {   struct f_parse f = p->f;
                struct lex l = p->l;
                p_frame_scan(p);
                p->l = l;

                if (p->f.call) {
                    p_expr(p);
                    encode(p, OP_CALL, p->sp-(p->params == 0xf ? 1 : p->params),
                           p->params, 0xf,
                           -(p->params == 0xf ? 1 : p->params));
                } else {
                    p->f.tabled = true;
                    p->f.unpack = false;
                    p_frame(p);
                }
                p->f = f;
            }
            expect(p, T_RTABLE);
            p->state = P_DIRECT;
            return p_postexpr(p);

        case T_EXPAND:
        case T_OP:
            encode(p, OP_IMM, p->sp+1, imm(p, mcstr("ops")), 0, +1);
            encode(p, OP_LOOKUP, p->sp, 0, p->sp, 0);
            encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
            encode(p, OP_LOOKDN, p->sp-1, p->sp-1, p->sp, -1);
            mu_lex(&p->l);
            p_subexpr(p);
            expect_direct(p, 0);
            p->params = 1;
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

        case T_NIL:
            mu_lex(&p->l);
            p->state = P_NIL;
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

static void p_postexpr(struct parse *p) {
    switch (p->l.tok) {
        case T_LPAREN:
            mu_lex(&p->l);
            expect_direct(p, 0);
            {   struct f_parse f = p->f;
                struct lex l = p->l;
                p_frame_scan(p);
                p->l = l;

                if (p->f.call) {
                    p_expr(p);
                    encode(p, OP_CALL, p->sp-(p->params == 0xf ? 1 : p->params),
                           p->params, 0xf,
                           -(p->params == 0xf ? 1 : p->params));
                    p->params = 0xf;
                } else {
                    p->f.unpack = false;
                    p_frame(p);
                    p->params = p->f.tabled ? 0xf : p->f.count;
                }
                p->f = f;
            }
            expect(p, T_RPAREN);
            p->state = P_CALLED;
            return p_postexpr(p);

        case T_EXPAND:
        case T_OP:
            if (p->l.lprec <= p->l.rprec)
                return;

            {   uintq_t lprec = p->l.lprec;
                p->l.lprec = p->l.rprec;
                mu_t sym = mu_inc(p->l.val);
                mu_lex(&p->l);
                expect_direct(p, 1);

                encode(p, OP_IMM, p->sp-1, imm(p, mcstr("ops")), 0, 0);
                encode(p, OP_LOOKUP, p->sp-1, 0, p->sp-1, 0);
                encode(p, OP_IMM, p->sp+1, imm(p, sym), 0, +1);
                encode(p, OP_LOOKDN, p->sp-2, p->sp-2, p->sp, -1);

                p_subexpr(p);
                expect_direct(p, 0);
                p->l.lprec = lprec;
            }
            p->params = 2;
            p->state = P_CALLED;
            return p_postexpr(p);

        case T_LTABLE:
            mu_lex(&p->l);
            expect_direct(p, 0);
            p_expr(p);
            expect_direct(p, 0);
            expect(p, T_RTABLE);
            p->state = P_INDIRECT;
            return p_postexpr(p);

        case T_DOT:
            mu_lex(&p->l);
            expect_direct(p, 0);
            encode(p, OP_IMM, p->sp+1, imm(p, p->l.val), 0, +1);
            expect(p, T_SYM); // TODO what constitutes a sym?
            p->state = P_INDIRECT;
            return p_postexpr(p);

        case T_ARROW:
            mu_lex(&p->l);
            expect_direct(p, 2);
            encode(p, OP_IMM, p->sp-1, imm(p, p->l.val), 0, 0);
            encode(p, OP_LOOKUP, p->sp-1, p->sp, p->sp-1, 0);
            expect(p, T_SYM); // TODO what constitutes a sym?
            encode(p, OP_IMM, p->sp-2, imm(p, mcstr("bind")), 0, 0);
            encode(p, OP_LOOKUP, p->sp-2, 0, p->sp-2, 0);
            encode(p, OP_CALL, p->sp-2, 2, 1, -2);
            p->state = P_DIRECT;
            return p_postexpr(p);

        case T_RPAREN:
            if (p->f.count == 0 && p->l.paren > p->f.paren) {
                mu_lex(&p->l);
                p->l.lprec = MU_MAXUINTQ;
                return p_postexpr(p);
            }
            return;

        default:
            return;
    }
}

static void p_expr(struct parse *p) {
    uintq_t lprec = p->l.lprec;
    uintq_t paren = p->f.paren;
    p->l.lprec = MU_MAXUINTQ;
    p->f.paren = p->l.paren;
    p_subexpr(p);
    p->l.lprec = lprec;
    p->f.paren = paren;
}

static void p_entry(struct parse *p) {
    p->f.key = false;

    if (p->l.tok == T_KEY) {
        encode(p, OP_IMM, p->sp+1, imm(p, mu_inc(p->l.val)), 0, +1);
        mu_lex(&p->l);
        p->state = P_DIRECT;
        p->f.key = true;
    } else if (!(p->f.unpack && p->l.tok == T_LTABLE)) {
        p_subexpr(p);
    }

    if (p->l.tok == T_PAIR) {
        mu_lex(&p->l);

        if (p->f.unpack && p->f.expand) {
            expect_direct(p, 1);
            encode(p, OP_IMM, p->sp+1, imm(p, mnil), 0, +1);
            encode(p, OP_LOOKUP, p->sp-2, p->sp-3, p->sp-1, 0);
            encode(p, OP_INSERT, p->sp, p->sp-3, p->sp-1, -2);
        } else if (p->f.unpack) {
            expect_direct(p, 0);
            encode(p, p->f.count == p->f.target-1 ? OP_LOOKDN : OP_LOOKUP,
                   p->sp, p->sp-1, p->sp, 0);
        } else {
            expect_direct(p, 0);
        }

        if (p->f.key && !mu_isexpr(p->l.tok)) {
            encode(p, OP_IMM, p->sp+1, imm(p, mu_inc(p->l.val)), 0, +1);
            p->state = P_SCOPED;
        } else if (!(p->f.unpack && p->l.tok == T_LTABLE)) {
            p_subexpr(p);
        }

        if (p->f.key)
            mu_dec(p->l.val);
        else
            p->f.key = true;
    } else if (p->f.tabled) {
        // TODO move this?
        int offset = p->state == P_INDIRECT ? 2
                   : p->state == P_SCOPED ? 1
                   : 0;

        if (p->f.unpack && p->f.expand) {
            encode(p, OP_IMM, p->sp+1, imm(p, mcstr("pop")), 0, +1);
            encode(p, OP_LOOKUP, p->sp, 0, p->sp, 0);
            encode(p, OP_DUP, p->sp+1, p->sp-1-offset, 0, +1);
            encode(p, OP_IMM, p->sp+1, imm(p, muint(p->f.index)), 0, +1);
            encode(p, OP_CALL, p->sp-2, 2, 1, -2);
        } else if (p->f.unpack) {
            encode(p, OP_IMM, p->sp+1, imm(p, muint(p->f.index)), 0, +1);
            encode(p, p->f.count == p->f.target-1 ? OP_LOOKDN : OP_LOOKUP,
                   p->sp, p->sp-1-offset, p->sp, 0);
        }
    } else {
        if (p->f.unpack && p->l.tok == T_LTABLE && p->f.count < p->f.target-1)
            encode(p, OP_MOVE, p->sp+1,
                   p->sp - (p->f.target-1 - p->f.count), 0, +1);
    }

    if (p->f.unpack && p->l.tok == T_LTABLE) {
        mu_lex(&p->l);
        struct f_parse f = p->f;
        struct lex l = p->l;
        p_frame_scan(p);
        p->l = l;

        p->f.unpack = true;
        p->f.tabled = true;
        p_frame(p);
        p->f = f;

        p->sp -= p->f.tabled || p->f.count < p->f.target-1;
        expect(p, T_RTABLE);
    } else if (p->f.unpack) {
        expect_indirect(p);

        if (p->f.key) {
            if (p->state == P_NIL) {
                encode(p, OP_DROP, p->sp, 0, 0, -1);
            } else if (p->state == P_SCOPED) {
                encode(p, p->f.insert ? OP_INSERT : OP_ASSIGN,
                       p->sp-1, 0, p->sp, -2);
            } else {
                encode(p, p->f.insert ? OP_INSERT : OP_ASSIGN,
                       p->sp-2, p->sp-1, p->sp, 0);
                encode(p, OP_DROP, p->sp-1, 0, 0, -3);
            }
        } else if (p->f.tabled) {
            if (p->state == P_NIL) {
                encode(p, OP_DROP, p->sp, 0, 0, -1);
            } else if (p->state == P_SCOPED) {
                encode(p, p->f.insert ? OP_INSERT : OP_ASSIGN,
                       p->sp, 0, p->sp-1, -2);
            } else {
                encode(p, p->f.insert ? OP_INSERT : OP_ASSIGN,
                       p->sp, p->sp-2, p->sp-1, 0);
                encode(p, OP_DROP, p->sp-2, 0, 0, -3);
            }
        } else {
            if (p->state == P_NIL) {
                encode(p, OP_DROP, p->sp - (p->f.target-1 - p->f.count),
                          0, 0, 0);
            } else if (p->state == P_SCOPED) {
                encode(p, p->f.insert ? OP_INSERT : OP_ASSIGN,
                       p->sp-1 - (p->f.target-1 - p->f.count),
                       0, p->sp, -1);
            } else {
                encode(p, p->f.insert ? OP_INSERT : OP_ASSIGN,
                       p->sp-2 - (p->f.target-1 - p->f.count),
                       p->sp-1, p->sp, 0);
                encode(p, OP_DROP, p->sp-1, 0, 0, -2);
            }
        }
    } else {
        expect_direct(p, 0);

        if (p->f.key) {
            encode(p, OP_INSERT, p->sp, p->sp-2, p->sp-1, -2);
        } else if (p->f.tabled) {
            encode(p, OP_IMM, p->sp+1, imm(p, muint(p->f.index)), 0, +1);
            encode(p, OP_INSERT, p->sp-1, p->sp-2, p->sp, -2);
        } else if (p->f.count >= p->f.target) {
            encode(p, OP_DROP, p->sp, 0, 0, -1);
        }
    }
}

static void p_frame(struct parse *p) {
    if (!p->f.unpack && p->f.tabled && !p->f.call &&
        !(p->f.expand && p->f.count == 0))
        encode(p, OP_TBL, p->sp+1, p->f.count, 0, +1);

    uintq_t lprec = p->l.lprec;
    uintq_t paren = p->f.paren;
    p->l.lprec = MU_MAXUINTQ;
    p->f.paren = p->l.paren;
    p->f.count = 0;
    p->f.index = 0;

    while (p->l.tok == T_LPAREN)
        mu_lex(&p->l);

    while (mu_isexpr(p->l.tok)) {
        if (p->l.tok == T_EXPAND) {
            mu_lex(&p->l);
            break;
        }

        p_entry(p);
        p->f.index += !p->f.key;
        p->f.count += 1;

        if (p->f.single || p->l.tok != T_SEP)
            break;

        mu_lex(&p->l);
    }

    while (p->l.paren > p->f.paren)
        expect(p, T_RPAREN);

    if (p->f.unpack) {
        if (p->f.expand) {
            p_subexpr(p);
            expect_indirect(p);

            // TODO combine this somehow?
            if (p->state == P_NIL) {
                encode(p, OP_DROP, p->sp, 0, 0, -1);
            } else if (p->state == P_SCOPED) {
                encode(p, p->f.insert ? OP_INSERT : OP_ASSIGN,
                       p->sp-1, 0, p->sp, -1);
            } else {
                encode(p, p->f.insert ? OP_INSERT : OP_ASSIGN,
                       p->sp-2, p->sp-1, p->sp, 0);
                encode(p, OP_DROP, p->sp-1, 0, 0, -2);
            }
        } else if (!p->f.tabled) {
            p->sp -= p->f.count;
        }
    } else {
        if (p->f.expand && p->f.tabled && p->f.count > 0) {
            encode(p, OP_MOVE, p->sp+1, p->sp, 0, +1);
            encode(p, OP_IMM, p->sp-1, imm(p, mcstr("concat")), 0, 0);
            encode(p, OP_LOOKUP, p->sp-1, 0, p->sp-1, 0);
            p_subexpr(p);
            expect_direct(p, 0);
            encode(p, OP_IMM, p->sp+1, imm(p, muint(p->f.index)), 0, +1);
            encode(p, OP_CALL, p->sp-3, 3, 1, -3);
        } else if (p->f.expand && p->f.tabled) {
            p_subexpr(p);
            expect_direct(p, 0);
        } else if (!p->f.tabled) {
            while (p->f.target > p->f.count) {
                encode(p, OP_IMM, p->sp+1, imm(p, mnil), 0, +1);
                p->f.count++;
            }
        }
    }

    p->l.lprec = lprec;
    p->f.paren = paren;
}

static void p_assignment(struct parse *p) {
    struct lex l = p->l;
    p_frame_scan(p);

    if (p->l.tok == T_ASSIGN) {
        mu_lex(&p->l);
        {   struct f_parse f = p->f;
            struct lex l = p->l;
            p_frame_scan(p);
            p->l = l;

            if ((p->f.count == 0 && !p->f.tabled) ||
                (f.count == 0 && !f.tabled)) {
                mu_err_parse(); // TODO better message
            } else if (p->f.call) {
                p_expr(p);
                encode(p, OP_CALL, p->sp-(p->params == 0xf ? 1 : p->params),
                       p->params, f.tabled ? 0xf : f.count,
                       +(f.tabled ? 1 : f.count)
                       -(p->params == 0xf ? 1 : p->params)-1);
            } else {
                p->f.unpack = false;
                p->f.tabled = p->f.tabled || f.tabled;
                p->f.target = f.count;
                p_frame(p);

                if (p->f.tabled && !f.tabled) {
                    encode(p, OP_MOVE, p->sp + p->f.target,
                           p->sp, 0, +p->f.target);

                    for (int i = 0; i < p->f.target; i++) {
                        encode(p, OP_IMM, p->sp-1 - (p->f.target-1 - i),
                               imm(p, muint(i)), 0, 0);
                        encode(p, i == p->f.target-1 ? OP_LOOKDN : OP_LOOKUP,
                               p->sp-1 - (p->f.target-1 - i), p->sp,
                               p->sp-1 - (p->f.target-1 - i),
                               -(p->f.target-1 == i));
                    }
                }
            }

            p->f = f;
        }
        p->l = l;

        p->f.unpack = true;
        p_frame(p);

        expect(p, T_ASSIGN);
        p_frame_scan(p);
    } else if (!p->f.insert) {
        p->l = l;

        if (p->f.call) {
            p_expr(p);
            encode(p, OP_CALL, p->sp-(p->params == 0xf ? 1 : p->params),
                   p->params, 0,
                   -(p->params == 0xf ? 1 : p->params)-1);
        } else {
            p->f.tabled = false;
            p->f.target = 0;
            p->f.unpack = false;
            p_frame(p);
        }
    } else {
        unexpected(p); // TODO better message
    }
}

static void p_stmt(struct parse *p) {
    switch (p->l.tok) {
        case T_LBLOCK:
            return p_block(p);

        case T_ARROW:
        case T_RETURN:
            mu_lex(&p->l);
            struct lex l = p->l;
            p_frame_scan(p);
            p->l = l;

            if (p->f.call) {
                p_expr(p);
                encode(p, OP_TCALL,
                       p->sp - (p->params == 0xf ? 1 : p->params),
                       p->params, 0,
                       -(p->params == 0xf ? 1 : p->params)-1);
            } else {
                p->f.unpack = false;
                p_frame(p);
                encode(p, OP_RET,
                       p->sp - (p->f.tabled ? 0 : p->f.count-1),
                       0, p->f.tabled ? 0xf : p->f.count,
                       -(p->f.tabled ? 1 : p->f.count));
            }
            return;

        case T_LET:
            mu_lex(&p->l);
            p->f.insert = true;
            return p_assignment(p);

        default:
            p->f.insert = false;
            return p_assignment(p);
    }
}

static void p_block(struct parse *p) {
    uintq_t block = p->l.block;
    uintq_t lparen = p->l.paren;
    uintq_t fparen = p->f.paren;
    p->l.paren = 0;
    p->f.paren = MU_MAXUINTQ;

    while (p->l.tok == T_LBLOCK)
        mu_lex(&p->l);

    do {
        while (true) {
            p_stmt(p);

            if (p->l.tok != T_TERM)
                break;

            mu_lex(&p->l);
        }

        if (p->l.block > block)
            expect(p, T_RBLOCK);
    } while (p->l.block > block);

    p->l.paren = lparen;
    p->f.paren = fparen;
}


struct code *mu_parse_expr(mu_t code) {
    struct parse *p = parse_create(code);
    // TODO find best entry point for expressions
    return parse_realize(p);
}

struct code *mu_parse_fn(mu_t code) {
    struct parse *p = parse_create(code);
    p_block(p);
    encode(p, OP_RET, 0, 0, 0, 0);
    return parse_realize(p);
}

struct code *mu_parse_module(mu_t code) {
    struct parse *p = parse_create(code);
    p_block(p);
    encode(p, OP_RET, 0, 0, 1, 0);
    return parse_realize(p);
}

