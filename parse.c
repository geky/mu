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
#define unexpected(p) _unexpected(p, __FUNCTION__)
static mu_noreturn void _unexpected(parse_t *p, const char *fn) {
//    printf("\033[31munexpected (%d) in %s\033[0m\n", p->tok, fn);
    mu_err_parse();
}

// Expect a token or fail
#define expect(p, tok) _expect(p, tok, __FUNCTION__)
static void _expect(parse_t *p, tok_t tok, const char *fn) {
    if (p->tok != tok) {
//        printf("\033[31mexpected (%d) not (%d) in %s\033[0m\n", tok, p->tok, fn);
        mu_err_parse();
    }
}

static uint_t imm(parse_t *p, mu_t v) {
    mu_t index = tbl_lookup(p->imms, v);

    if (!isnil(index))
        return getuint(index);

    uint_t arg = tbl_getlen(p->imms);
    tbl_assign(p->imms, v, muint(arg));
    return arg;
}

static struct chunk *chunk_create(void) {
    struct chunk *ch = (struct chunk *)mstr_create(
        mu_offset(struct chunk, data) - mu_offset(mstr_t, data));

    ch->len = 0;
    ch->indirect = false;

    return ch;
}

static void emit(struct chunk **ch, data_t byte) {
    mstr_insert((mstr_t **)ch, (*ch)->len + mu_offset(struct chunk, data) 
                               - mu_offset(mstr_t, data), byte);
    (*ch)->len += 1;
}

static void merge(struct chunk **ch, data_t *data, uint_t len) {
    mstr_nconcat((mstr_t **)ch, (*ch)->len + mu_offset(struct chunk, data)
                                - mu_offset(mstr_t, data), data, len);
    (*ch)->len += len;
}

static void encode(parse_t *p, op_t op, uint_t arg) {
    mu_encode((void (*)(void *, data_t))emit, &p->ch, op, arg);
}

static parse_t *parse_create(str_t *code) {
    parse_t *p = mu_alloc(sizeof(parse_t));

    p->ch = chunk_create();

    p->imms = tbl_create(0);
    p->fns = tbl_create(0);

    p->pos = str_getdata(code);
    p->end = str_getdata(code) + str_getlen(code);

    mu_lex(p);

    return p;
}


static uint_t fpack(parse_t *p, tbl_t *keys, tbl_t *vals) {
    encode(p, OP_TBL, tbl_getlen(vals));

    tbl_for_begin (i, v, vals) {
        mu_t k = tbl_lookup(keys, i);

        if (isnil(k)) {
            encode(p, OP_IMM, imm(p, i));
        } else if (isnum(k)) {
            fprintf(stderr, "oh no!");
            assert(false);
        } else {
            struct chunk *kch = (struct chunk *)getstr(k);
            merge(&p->ch, kch->data, kch->len);
        }

        struct chunk *vch = (struct chunk *)getstr(v);
        merge(&p->ch, vch->data, vch->len);
            
        if (vch->indirect)
            encode(p, OP_LOOKUP, 0);

        encode(p, OP_FINSERT, 0);
    } tbl_for_end

    tbl_dec(keys);
    tbl_dec(vals);
    return 0xf;
}

static c_t lpack(parse_t *p, tbl_t *keys, tbl_t *vals) {
    uint_t c = 0;

    tbl_for_begin (i, v, vals) {
        mu_t k = tbl_lookup(keys, i);

        if (!isnil(k)) {
            struct chunk *kch = (struct chunk *)getstr(k);
            merge(&p->ch, kch->data, kch->len);
        }

        struct chunk *vch = (struct chunk *)getstr(v);
        merge(&p->ch, vch->data, vch->len);
            
        if (vch->indirect)
            encode(p, OP_LOOKUP, 0);

        if (isnil(k)) {
            if (getuint(i) > MU_FRAME) {
                encode(p, OP_DROP, 1);
            } else {
                c += 1;
            }
        } else {
            encode(p, OP_DROP, 2);
        }
    } tbl_for_end

    tbl_dec(keys);
    tbl_dec(vals);

    return c;
}

static c_t pack(parse_t *p, tbl_t *keys, tbl_t *vals) {
    if (tbl_getlen(vals) > MU_FRAME || tbl_getlen(keys) > 0)
        return fpack(p, keys, vals);
    else
        return lpack(p, keys, vals);
}

static void funpack(parse_t *p, op_t op, tbl_t *keys, tbl_t *vals) {
    tbl_for_begin (i, v, vals) {
        mu_t k = tbl_lookup(keys, i);

        if (isnil(k)) {
            encode(p, OP_IMM, imm(p, i));
        } else {
            struct chunk *kch = (struct chunk *)getstr(k);
            merge(&p->ch, kch->data, kch->len);
        }

        encode(p, OP_FLOOKUP, 0);

        if (istbl(v)) {
            funpack(p, op, gettbl(mu_lookup(v, muint(0))),
                           gettbl(mu_lookup(v, muint(1))));
        } else {
            struct chunk *vch = (struct chunk *)getstr(v);
            
            if (!vch->indirect)
                mu_err_parse(); // can't assign direct value

            merge(&p->ch, vch->data, vch->len);
            encode(p, op, 0);
        }
    } tbl_for_end

    encode(p, OP_DROP, 1);

    tbl_dec(keys);
    tbl_dec(vals);
}

static void unpack(parse_t *p, op_t op, tbl_t *keys, tbl_t *vals) {
    len_t len = tbl_getlen(vals);

    if (len > MU_FRAME || tbl_getlen(keys) > 0)
        return funpack(p, op, keys, vals);

    tbl_for_begin (i, v, vals) {
        if (istbl(v)) {
            funpack(p, op, gettbl(mu_lookup(v, muint(0))),
                           gettbl(mu_lookup(v, muint(1))));
        } else {
            struct chunk *vch = (struct chunk *)getstr(v);
            
            if (!vch->indirect)
                mu_err_parse(); // can't assign direct value

            merge(&p->ch, vch->data, vch->len);
            encode(p, op, len-1 - getuint(i));
        }
    } tbl_for_end

    tbl_dec(keys);
    tbl_dec(vals);
}

static void assign(parse_t *p, op_t op, tbl_t *lkeys, tbl_t *lvals,
                                        tbl_t *rkeys, tbl_t *rvals) {
    len_t llen = tbl_getlen(lvals);

    if (llen > MU_FRAME || tbl_getlen(lkeys) > 0) {
        fpack(p, rkeys, rvals);
        funpack(p, op, lkeys, lvals);
    } else {
        len_t rlen = lpack(p, rkeys, rvals);

        if (rlen > llen)
            encode(p, OP_DROP, rlen - llen);
        else if (llen > rlen)
            encode(p, OP_PAD, llen - rlen);

        unpack(p, op, lkeys, lvals);
    }
}

static code_t *parse_realize(parse_t *p) {
    struct code *code = ref_alloc(
        sizeof(struct code) + 
        sizeof(mu_t)*tbl_getlen(p->imms) +
        sizeof(struct code *)*tbl_getlen(p->fns) +
        p->ch->len);

    code->bcount = p->ch->len;
    code->icount = tbl_getlen(p->imms);
    code->fcount = tbl_getlen(p->fns);

    code->flags.stack = 25; // TODO
    code->flags.scope = 5; // TODO
    code->flags.args = 0xf; // TODO
    code->flags.type = 0;

    mu_t *imms = code_imms(code);
    struct code **fns = code_fns(code);
    data_t *bcode = (data_t *)code_bcode(code);

    tbl_for_begin (k, v, p->imms) {
        imms[getuint(v)] = mu_inc(k);
    } tbl_for_end;

//  TODO
//    tbl_for_begin (k, v, p->fns) {
//        fns[getuint(k)] = mu_inc(v);
//    } tbl_for_end;

    tbl_dec(p->imms);
    tbl_dec(p->fns);

    memcpy(bcode, p->ch->data, p->ch->len);

    mu_dealloc(p->ch, mu_offset(struct chunk, data) + p->ch->size);
    mu_dealloc(p, sizeof(parse_t));

    return code;
}




static void p_postexpr(parse_t *p);
static void p_expr(parse_t *p);
static void p_postkey(parse_t *p);
static void p_entry(parse_t *p);
static void p_entry_list(parse_t *p);
static void p_stmt(parse_t *p);
static void p_stmt_list(parse_t *p);




static void p_expr(parse_t *p) {
    switch (p->tok) {
        case T_SYM:     encode(p, OP_SYM, imm(p, p->val));
                        p->ch->indirect = true;
                        mu_lex(p);
                        break;

        case T_IMM:     encode(p, OP_IMM, imm(p, p->val));
                        p->ch->indirect = false;
                        mu_lex(p);
                        break;

        case '[':       {   tbl_t *keys = p->keys;
                            tbl_t *vals = p->vals;
                            mu_lex(p);
                            p_entry_list(p);
                            fpack(p, p->keys, p->vals);
                            expect(p, ']');
                            p->keys = keys;
                            p->vals = vals;
                        }
                        p->ch->indirect = false;
                        break;

        default:        unexpected(p);
    }

    // postexpr here
}

static void p_entry(parse_t *p) {
    switch (p->tok) {
        case T_KEY:     encode(p, OP_IMM, imm(p, p->val));
                        mu_lex(p);
                        break;

        case T_REST:    tbl_insert(p->keys, muint(tbl_getlen(p->vals)), minf);
                        mu_lex(p);
                        p_expr(p);
                        return;

        case T_IMM:
        case T_SYM:     
        case '[':       p_expr(p);
                        break;

        default:        return;
    }

    switch (p->tok) {
        case ':':       if (p->ch->indirect)
                            encode(p, OP_LOOKUP, 0);
                        tbl_insert(p->keys, muint(tbl_getlen(p->vals)), 
                                            mchunk(p->ch));
                        p->ch = chunk_create();
                        mu_lex(p);
                        p_expr(p);
                        break;

        default:        break;
    }
}

static void p_entry_list(parse_t *p) {
    struct chunk *ch = p->ch;
    p->keys = tbl_create(0);
    p->vals = tbl_create(0);

    while (true) {
        p->ch = chunk_create();
        p_entry(p);
        tbl_insert(p->vals, muint(tbl_getlen(p->vals)), mchunk(p->ch));

        if (p->tok != ',')
            break;

        mu_lex(p);
    }

    p->ch = ch;
}

static void p_stmt(parse_t *p) {
    switch (p->tok) {
        case T_RETURN:  mu_lex(p);
                        p_entry_list(p);
                        encode(p, OP_RET, pack(p, p->keys, p->vals));
                        return;

        case T_LET:     mu_lex(p);
                        p_entry_list(p);
                        expect(p, '=');
                        {   tbl_t *lkeys = p->keys;
                            tbl_t *lvals = p->vals;
                            mu_lex(p);
                            p_entry_list(p);
                            assign(p, OP_INSERT, lkeys, lvals, p->keys, p->vals);
                        }
                        return;

        default:        p_entry_list(p);
/*                        if (p->tok == T_OPSET && tbl_getlen(p->vals) == 1) {
                            // TODO operator assignments?
                        } else*/  if (p->tok == '=') {
                            tbl_t *lkeys = p->keys;
                            tbl_t *lvals = p->vals;
                            mu_lex(p);
                            p_entry_list(p);
                            assign(p, OP_ASSIGN, lkeys, lvals, p->keys, p->vals);
                        } else {
                            uint_t n = pack(p, p->keys, p->vals);
                            encode(p, OP_DROP, n > MU_FRAME ? 1 : n);
                        }
                        return;
    }
}

static void p_stmt_list(parse_t *p) {
    while (true) {
        p_stmt(p);

        if (p->tok != ';')
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
    encode(p, OP_RET, 0);
    return parse_realize(p);
}

code_t *mu_parse_module(str_t *code) {
    parse_t *p = parse_create(code);
    p_stmt_list(p);
    // TODO get scope
    return parse_realize(p);
}

