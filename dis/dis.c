/*
 * Mu virtual machine
 *
 * Copyright (c) 2016 Christopher Haster
 * Distributed under the MIT license in mu.h
 */
#include "dis.h"
#include "mu/mu.h"


// Disassemble bytecode for debugging and introspection
// currently outputs to stdout
static const char *const op_names[16] = {
    [MU_OP_IMM]    = "imm",
    [MU_OP_FN]     = "fn",
    [MU_OP_TBL]    = "tbl",
    [MU_OP_MOVE]   = "move",
    [MU_OP_DUP]    = "dup",
    [MU_OP_DROP]   = "drop",
    [MU_OP_LOOKUP] = "lookup",
    [MU_OP_LOOKDN] = "lookdn",
    [MU_OP_INSERT] = "insert",
    [MU_OP_ASSIGN] = "assign",
    [MU_OP_JUMP]   = "jump",
    [MU_OP_JTRUE]  = "jtrue",
    [MU_OP_JFALSE] = "jfalse",
    [MU_OP_CALL]   = "call",
    [MU_OP_TCALL]  = "tcall",
    [MU_OP_RET]    = "ret",
};

static mu_t mu_dis_summu(mu_t m) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_pushf(&b, &n, " (%r", m);
    if (n > 8+2) {
        n = 8;
        mu_buf_pushf(&b, &n, "..");
    }
    mu_buf_pushf(&b, &n, ")");
    mu_buf_resize(&b, n);
    return b;
}

static mu_t mu_dis_sumpair(mu_t k, mu_t v) {
    mu_t b = mu_buf_create(0);
    muint_t n = 0;
    mu_buf_pushf(&b, &n, " (%r", k);
    if (n > 8+2) {
        n = 8;
        mu_buf_pushf(&b, &n, "..");
    }
    muint_t oldn = n;
    mu_buf_pushf(&b, &n, ": %r", v);
    if (n > oldn + 8+2) {
        n = oldn + 8;
        mu_buf_pushf(&b, &n, "..");
    }
    mu_buf_pushf(&b, &n, ")");
    mu_buf_resize(&b, n);
    return b;
}

static void mu_dis_code(mu_t c) {
    mu_t *imms = mu_code_getimms(c);
    const uint16_t *start = mu_code_getbcode(c);
    const uint16_t *pc = mu_code_getbcode(c);
    const uint16_t *end = pc + mu_code_getbcodelen(c)/2;

    mu_printf("regs: %qu, locals: %qu, args: 0x%bx",
            mu_code_getregs(c),
            mu_code_getlocals(c),
            mu_code_getargs(c));

    if (mu_code_getimmslen(c) > 0) {
        mu_printf("imms:");
        for (muint_t i = 0; i < mu_code_getimmslen(c); i++) {
            mu_printf("%hx  %t%m", i, mu_inc(imms[i]),
                    mu_dis_summu(mu_inc(imms[i])));
        }
    }

    mu_printf("bcode:");
    while (pc < end) {
        mop_t op = pc[0] >> 12;

        if (op == MU_OP_DROP) {
            mu_printf("%hx  %bx%bx      %s r%d", pc - start,
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8));
            pc += 1;
        } else if (op >= MU_OP_RET && op <= MU_OP_DROP) {
            mu_printf("%hx  %bx%bx      %s r%d, 0x%bx", pc - start,
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), 0xff & pc[0]);
            pc += 1;
        } else if (op >= MU_OP_LOOKDN && op <= MU_OP_ASSIGN) {
            mu_printf("%hx  %bx%bx      %s r%d, r%d[r%d]", pc - start,
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), 0xf & (pc[0] >> 4), 0xf & pc[0]);
            pc += 1;
        } else if (op == MU_OP_IMM && (0xff & pc[0]) == 0xff) {
            mu_printf("%hx  %bx%bx%bx%bx  %s r%d, %u%m", pc - start,
                    pc[0] >> 8, 0xff & pc[0],
                    pc[1] >> 8, 0xff & pc[1], op_names[op],
                    0xf & (pc[0] >> 8), pc[1],
                    mu_dis_summu(mu_inc(imms[pc[1]])));
            pc += 2;
        } else if (op == MU_OP_IMM) {
            mu_printf("%hx  %bx%bx      %s r%d, %u%m", pc - start,
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), 0x7f & pc[0],
                    mu_dis_summu(mu_inc(imms[0x7f & pc[0]])));
            pc += 1;
        } else if (op >= MU_OP_IMM && op <= MU_OP_TBL
                && (0xff & pc[0]) == 0xff) {
            mu_printf("%hx  %bx%bx%bx%bx  %s r%d, %u", pc - start,
                    pc[0] >> 8, 0xff & pc[0],
                    pc[1] >> 8, 0xff & pc[1], op_names[op],
                    0xf & (pc[0] >> 8), pc[1]);
            pc += 2;
        } else if (op >= MU_OP_IMM && op <= MU_OP_TBL) {
            mu_printf("%hx  %bx%bx      %s r%d, %u", pc - start,
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), 0x7f & pc[0]);
            pc += 1;
        } else if (op >= MU_OP_JFALSE && op <= MU_OP_JUMP
                && (0xff & pc[0]) == 0xff) {
            mu_printf("%hx  %bx%bx%bx%bx  %s r%d, %d (%hx)", pc - start,
                    pc[0] >> 8, 0xff & pc[0],
                    pc[1] >> 8, 0xff & pc[1], op_names[op],
                    0xf & (pc[0] >> 8), (int16_t)pc[1],
                    ((int16_t)pc[1]) + pc+2 - start);
            pc += 2;
        } else if (op >= MU_OP_JFALSE && op <= MU_OP_JUMP) {
            mu_printf("%hx  %bx%bx      %s r%d, %d (%hx)", pc - start,
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), (int16_t)(pc[0] << 8) >> 8,
                    ((int16_t)(pc[0] << 8) >> 8) + pc+1 - start);
            pc += 1;
        }
    }
}

static void mu_dis_bufdata(mu_t m) {
    mu_t line = mu_buf_create(80);
    muint_t maxwidth = 0;

    for (muint_t i = 0; i < mu_buf_getlen(m); i += 16) {
        muint_t n = 0;
        mu_buf_pushf(&line, &n, "%hx  ", i);
        for (muint_t j = 0; j < 16 && i+j < mu_buf_getlen(m); j++) {
            mu_buf_pushf(&line, &n, "%bx ",
                    ((mbyte_t *)mu_buf_getdata(m))[i+j]);
        }

        if (n > maxwidth) {
            maxwidth = n;
        }

        while (n < maxwidth+1) {
            mu_buf_pushf(&line, &n, " ");
        }

        for (muint_t j = 0; j < 16 && i+j < mu_buf_getlen(m); j++) {
            mbyte_t c = ((mbyte_t *)mu_buf_getdata(m))[i+j];
            mu_buf_pushc(&line, &n, (c >= ' ' && c <= '~') ? c : '.');
        }

        mu_print(mu_buf_getdata(line), n);
    }

    mu_dec(line);
}

static void mu_dis_num(mu_t m) {
    muint_t x = (muint_t)m;
#ifdef MU64
    mu_printf("sign: %wx (%c1)",
            ((0x8000000000000000 & (muint_t)x)), 
            ((0x8000000000000000 & (muint_t)x) >> 63) ? '-' : '+');
    mu_printf("exp:  %wx (2^%wd)",
            ((0x7ff0000000000000 & (muint_t)x)),
            ((0x7ff0000000000000 & (muint_t)x) >> 52) - 0x3ff);
    mu_printf("mant: %wx (%r)",
            ((0x000ffffffffffff8 & (muint_t)x)),
            ((0x000ffffffffffff8 & (muint_t)x)) | 0x3ff0000000000000 | MTNUM);
#else
    mu_printf("sign: %wx (%c1)",
            ((0x80000000 & (muint_t)x)), 
            ((0x80000000 & (muint_t)x) >> 31) ? '-' : '+');
    mu_printf("exp:  %wx (2^%wd)",
            ((0x7ff00000 & (muint_t)x)),
            ((0x7ff00000 & (muint_t)x) >> 23) - 0x7f);
    mu_printf("mant: %wx (%r)",
            ((0x007ffff8 & (muint_t)x)),
            ((0x007ffff8 & (muint_t)x)) | 0x7f000000 | MTNUM);
#endif
    mu_printf("value: %r (%m)", m, mu_fn_call(MU_HEX, 0x11, m));
}

static void mu_dis_str(mu_t m) {
    mu_printf("ref: %d, len: %d", mu_getref(m), mu_str_getlen(m));
    mu_printf("data:");
    mu_dis_bufdata(m);
}

static void mu_dis_buf(mu_t m) {
    mu_printf("ref: %d, len: %d", mu_getref(m), mu_buf_getlen(m));
    mu_printf("dtor: 0x%wx", mu_buf_getdtor(m));
    mu_printf("tail: %t", mu_buf_gettail(m));
    mu_printf("data:");
    mu_dis_bufdata(m);
}

static void mu_dis_tbl(mu_t m) {
    struct mtbl *t = (struct mtbl *)((muint_t)m & ~7);
    mu_printf("ref: %hu, len: %hu, nils: %hu",
            t->ref, t->len, t->nils);
    mu_printf("npw2: %qu, isize: %qu, ro: %d",
            t->npw2, t->isize, mu_gettype(m) == MTRTBL);
    mu_printf("tail: %t", mu_inc(t->tail));

    if (t->isize == 0) {
        mu_printf("array:");
        for (muint_t i = 0; i < t->len+t->nils; i++) {
            mu_printf("%hx  %t%m", i, mu_inc(t->array[i]),
                    mu_dis_summu(mu_inc(t->array[i])));
        }
    } else {
        mu_printf("indices:");
        mu_t line = mu_buf_create(80);
        for (muint_t i = 0; i < (1 << t->npw2); i += 16) {
            muint_t n = 0;
            mu_buf_pushf(&line, &n, "%hx  ", i);
            for (muint_t j = 0; j < 16/t->isize && i+j < (1 << t->npw2); j++) {
#ifdef MU64
                if (t->isize == 1) {
                    mu_buf_pushf(&line, &n, "%bx ", ((mbyte_t*)t->array)[i+j]);
                } else if (t->isize == 2) {
                    mu_buf_pushf(&line, &n, "%qx ", ((muintq_t*)t->array)[i+j]);
                } else if (t->isize == 4) {
                    mu_buf_pushf(&line, &n, "%hx ", ((muinth_t*)t->array)[i+j]);
                } else if (t->isize == 8) {
                    mu_buf_pushf(&line, &n, "%wx ", ((muint_t*)t->array)[i+j]);
                }
#else
                if (t->isize == 1) {
                    mu_buf_pushf(&line, &n, "%bx ", ((mbyte_t*)t->array)[i+j]);
                } else if (t->isize == 2) {
                    mu_buf_pushf(&line, &n, "%hx ", ((muinth_t*)t->array)[i+j]);
                } else if (t->isize == 4) {
                    mu_buf_pushf(&line, &n, "%wx ", ((muint_t*)t->array)[i+j]);
                }
#endif
            }

            mu_print(mu_buf_getdata(line), n);
        }
        mu_dec(line);

        mu_printf("array:");
        muint_t off = (t->isize*(1 << t->npw2) +
                (2*sizeof(mu_t))-1) / (2*sizeof(mu_t));

        for (muint_t i = 0; i < t->len+t->nils; i++) {
            mu_printf("%hx  %t %t%m", i,
                    mu_inc(t->array[2*(off+i)+0]),
                    mu_inc(t->array[2*(off+i)+1]),
                    mu_dis_sumpair(
                        mu_inc(t->array[2*(off+i)+0]),
                        mu_inc(t->array[2*(off+i)+1])));
        }
    }
}

static void mu_dis_fn(mu_t m) {
    struct mfn *f = (struct mfn *)((muint_t)m & ~7);
    mu_printf("ref: %hu, args: 0x%bx, weak: %d",
            f->ref, f->args, (MU_FN_WEAK & f->flags) == MU_FN_WEAK);
    mu_printf("closure: %t", mu_inc(f->closure));

    if ((MU_FN_SCOPED & f->flags) && (MU_FN_BUILTIN & f->flags)) {
        mu_printf("sbfn: 0x%wx", f->fn.sbfn);
    } else if (MU_FN_BUILTIN & f->flags) {
        mu_printf("bfn: 0x%wx", f->fn.bfn);
    } else {
        mu_dis_code(f->fn.code);
    }
}

void mu_dis(mu_t m) {
    mu_printf("-- %t --", mu_inc(m));
    switch (mu_gettype(m)) {
        case MTNIL:  return;
        case MTNUM:  mu_dis_num(m); return;
        case MTSTR:  mu_dis_str(m); return;
        case MTBUF:
        case MTDBUF: mu_dis_buf(m); return;
        case MTTBL:
        case MTRTBL: mu_dis_tbl(m); return;
        case MTFN:   mu_dis_fn(m); return;
    }
}

static mcnt_t mu_dis_bfn(mu_t *frame) {
    mu_dis(frame[0]);
    mu_dec(frame[0]);
    return 0;
}

MU_DEF_STR(mu_dis_key_def, "dis")
MU_DEF_BFN(mu_dis_def, 0x1, mu_dis_bfn)
MU_DEF_TBL(mu_dis_module_def, {
    { mu_dis_key_def, mu_dis_def },
})
