#include "vm.h"

#include "mu.h"
#include "parse.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"


// Emitted jump size in bytes (7 bits per byte)
#ifndef MU_JUMP_SIZE
#ifdef MU64
#define MU_JUMP_SIZE 3
#else
#define MU_JUMP_SIZE 2
#endif
#endif


// Bytecode errors
static mu_noreturn mu_error_bytecode(void) {
    mu_errorf("exceeded bytecode limits");
}


// Encode the specified opcode and return its size
// Encode the specified opcode and return its size
// Note: size of the jump opcodes currently can not change based on argument
void mu_encode(void (*emit)(void *, mbyte_t), void *p,
               enum op op, mint_t d, mint_t a, mint_t b) {
    if (op > 0xf || d > 0xf)
        mu_error_bytecode();

    emit(p, (op << 4) | d);

    if (op >= OP_RET && op <= OP_DUP) {
        if (a > 0xff)
            mu_error_bytecode();

        emit(p, a);
    } else if (op >= OP_LOOKDN && op <= OP_ASSIGN) {
        if (a > 0xf || b > 0xf)
            mu_error_bytecode();

        emit(p, (a << 4) | b);
    } else if (op >= OP_IMM && op <= OP_TBL) {
        muint_t count = (a == 0) ? 1 : mu_npw2(a)/7 + 1;

        for (muint_t i = 0; i < count-1; i++) {
            emit(p, 0x80 | (0x7f & (a >> 7*(count-1-i))));
        }

        emit(p, 0x7f & a);
    } else if (op >= OP_JFALSE && op <= OP_JUMP) {
        a -= MU_JUMP_SIZE + 1;
        if (a > +(1 << (7*MU_JUMP_SIZE-1))-1 ||
            a < -(1 << (7*MU_JUMP_SIZE-1))) {
            mu_error_bytecode();
        }

        for (muint_t i = 0; i < MU_JUMP_SIZE-1; i++) {
            emit(p, 0x80 | (0x7f & (a >> 7*(MU_JUMP_SIZE-1-i))));
        }

        emit(p, 0x7f & a);
    }
}

mint_t mu_patch(void *c, mint_t nj) {
    mbyte_t *p = c;
    mu_assert((p[0] >> 4) >= OP_JFALSE && (p[0] >> 4) <= OP_JUMP);

    mint_t pj = (0x40 & *p) ? -1 : 0;
    for (muint_t i = 0; i < MU_JUMP_SIZE; i++) {
        pj = (pj << 7) | (0x7f & p[i+1]);
    }

    nj -= MU_JUMP_SIZE + 1;
    if (nj > +(1 << (7*MU_JUMP_SIZE-1))-1 ||
        nj < -(1 << (7*MU_JUMP_SIZE-1))) {
        mu_error_bytecode();
    }

    for (muint_t i = 0; i < MU_JUMP_SIZE-1; i++) {
        p[i+1] = 0x80 | (0x7f & (nj >> 7*(MU_JUMP_SIZE-1-i)));
    }
    p[MU_JUMP_SIZE] = 0x7f & nj;

    return pj + MU_JUMP_SIZE + 1;
}


// Disassemble bytecode for debugging and introspection
// currently outputs to stdout
static const char *const op_names[16] = {
    [OP_IMM]    = "imm",
    [OP_FN]     = "fn",
    [OP_TBL]    = "tbl",
    [OP_MOVE]   = "move",
    [OP_DUP]    = "dup",
    [OP_DROP]   = "drop",
    [OP_LOOKUP] = "lookup",
    [OP_LOOKDN] = "lookdn",
    [OP_INSERT] = "insert",
    [OP_ASSIGN] = "assign",
    [OP_JUMP]   = "jump",
    [OP_JTRUE]  = "jtrue",
    [OP_JFALSE] = "jfalse",
    [OP_CALL]   = "call",
    [OP_TCALL]  = "tcall",
    [OP_RET]    = "ret",
};

static mu_t mu_dis_bytes(muint_t count, const mbyte_t *pc) {
    mu_t b = buf_create(0);
    muint_t n = 0;
    muint_t i = 0;

    for (; i < count; i++) {
        buf_format(&b, &n, "%bx", pc[i]);
    }

    for (; i < MU_JUMP_SIZE+1; i++) {
        buf_format(&b, &n, "  ");
    }

    buf_resize(&b, n);
    return b;
}

void mu_dis(struct code *code) {
    mu_t *imms = code_imms(code);
    struct code **fns = code_fns(code);
    const mbyte_t *pc = code_bcode(code);
    const mbyte_t *end = pc + code->bcount;

    mu_printf("-- dis 0x%wx --", code);
    mu_printf("regs: %qu, scope: %qu, args: %bx",
           code->regs, code->scope, code->args);

    if (code->icount > 0) {
        mu_printf("imms:");
        for (muint_t i = 0; i < code->icount; i++) {
            mu_printf("%wx (%r)", imms[i], mu_inc(imms[i]));
        }
    }

    if (code->fcount > 0) {
        mu_printf("fns:");
        for (muint_t i = 0; i < code->fcount; i++) {
            mu_printf("%wx", fns[i]);
        }
    }

    mu_printf("bcode:");
    while (pc < end) {
        enum op op = *pc >> 4;

        if (op == OP_DROP) {
            mu_printf("%m %s r%d", 
                    mu_dis_bytes(1, pc),
                    op_names[op],
                    (0xf & pc[0]));
            pc += 1;
        } else if (op >= OP_RET && op <= OP_DUP) {
            mu_printf("%m %s r%d,0x%bx",
                    mu_dis_bytes(2, pc),
                    op_names[op],
                    (0xf & pc[0]),
                    pc[1]);
            pc += 2;
        } else if (op >= OP_LOOKDN && op <= OP_ASSIGN) {
            mu_printf("%m %s r%d,r%d[r%d]",
                    mu_dis_bytes(2, pc),
                    op_names[op],
                    (0xf & pc[0]),
                    (0xf & pc[1] >> 4),
                    (0xf & pc[1]));
            pc += 2;
        } else if (op >= OP_IMM && op <= OP_TBL) {
            unsigned d = 0xf & pc[0];
            muint_t i = 0;
            muint_t c = 0;
            do {
                i = (i << 7) | (0x7f & pc[c+1]);
            } while (0x80 & pc[c++ + 1]);

            if (op == OP_IMM) {
                mu_printf("%m %s r%d,%wu (%r)",
                        mu_dis_bytes(c+1, pc),
                        op_names[op], d, i,
                        mu_inc(imms[i]));
            } else {
                mu_printf("%m %s r%d,%wu",
                        mu_dis_bytes(c+1, pc),
                        op_names[op], d, i);
            }
            pc += c+1;
        } else if (op >= OP_JFALSE && op <= OP_JUMP) {
            unsigned d = 0xf & pc[0];
            mint_t j = (0x40 & pc[1]) ? -1 : 0;
            muint_t c = 0;
            do {
                j = (j << 7) | (0x7f & pc[c+1]);
            } while (0x80 & pc[c++ + 1]);

            if (op != OP_JUMP) {
                mu_printf("%m %s r%d,%wd",
                        mu_dis_bytes(c+1, pc),
                        op_names[op], d, j);
            } else {
                mu_printf("%m %s %wd",
                        mu_dis_bytes(c+1, pc),
                        op_names[op], j);
            }
            pc += c+1;
        }
    }
}


// Virtual machine dispatch macros
#define MU_COMPUTED_GOTO
#ifdef MU_COMPUTED_GOTO
#define VM_DISPATCH(pc)                         \
    {   static void *const vm_entry[16] = {     \
            [OP_IMM]    = &&VM_ENTRY_OP_IMM,    \
            [OP_FN]     = &&VM_ENTRY_OP_FN,     \
            [OP_TBL]    = &&VM_ENTRY_OP_TBL,    \
            [OP_MOVE]   = &&VM_ENTRY_OP_MOVE,   \
            [OP_DUP]    = &&VM_ENTRY_OP_DUP,    \
            [OP_DROP]   = &&VM_ENTRY_OP_DROP,   \
            [OP_LOOKUP] = &&VM_ENTRY_OP_LOOKUP, \
            [OP_LOOKDN] = &&VM_ENTRY_OP_LOOKDN, \
            [OP_INSERT] = &&VM_ENTRY_OP_INSERT, \
            [OP_ASSIGN] = &&VM_ENTRY_OP_ASSIGN, \
            [OP_JUMP]   = &&VM_ENTRY_OP_JUMP,   \
            [OP_JTRUE]  = &&VM_ENTRY_OP_JTRUE,  \
            [OP_JFALSE] = &&VM_ENTRY_OP_JFALSE, \
            [OP_CALL]   = &&VM_ENTRY_OP_CALL,   \
            [OP_TCALL]  = &&VM_ENTRY_OP_TCALL,  \
            [OP_RET]    = &&VM_ENTRY_OP_RET,    \
        };                                      \
                                                \
        while (1) {                             \
            goto *vm_entry[*pc >> 4];
#define VM_DISPATCH_END                         \
        }                                       \
        mu_unreachable;                         \
    }

#define VM_ENTRY(op)                            \
    VM_ENTRY_##op: {
#define VM_ENTRY_END                            \
        goto *vm_entry[*pc >> 4];               \
    }
#else
#define VM_DISPATCH(pc)             \
    {                               \
        while (1) {                 \
            switch (*pc >> 4) {
#define VM_DISPATCH_END             \
            }                       \
        }                           \
        mu_unreachable;             \
    }

#define VM_ENTRY(op)                \
    case op: {
#define VM_ENTRY_END                \
        break;                      \
    }
#endif


#define VM_ENTRY_D(op, d)                                       \
    VM_ENTRY(op)                                                \
        mu_unused register unsigned d = 0xf & *pc++;

#define VM_ENTRY_DA(op, d, a)                                   \
    VM_ENTRY(op)                                                \
        mu_unused register unsigned d = 0xf & *pc++;            \
        mu_unused register unsigned a = *pc++;

#define VM_ENTRY_DAB(op, d, a, b)                               \
    VM_ENTRY(op)                                                \
        mu_unused register unsigned d = 0xf & *pc++;            \
        mu_unused register unsigned a = 0xf & *pc >> 4;         \
        mu_unused register unsigned b = 0xf & *pc++;

#define VM_ENTRY_DI(op, d, i)                                   \
    VM_ENTRY(op)                                                \
        mu_unused register unsigned d = 0xf & *pc++;            \
        mu_unused register muint_t i = 0;                       \
        do {                                                    \
            i = (i << 7) | (0x7f & *pc);                        \
        } while (0x80 & *pc++);

#define VM_ENTRY_DJ(op, d, j)                                   \
    VM_ENTRY(op)                                                \
        mu_unused register unsigned d = 0xf & *pc++;            \
        mu_unused register mint_t j = (0x40 & *pc) ? -1 : 0;    \
        do {                                                    \
            j = (j << 7) | (0x7f & *pc);                        \
        } while (0x80 & *pc++);



// Execute bytecode
mc_t mu_exec(struct code *code, mu_t scope, mu_t *frame) {
    // Allocate temporary variables
    register const mbyte_t *pc;
    mu_t *imms;
    struct code **fns;

    register mu_t scratch;

reenter:
    {   // Setup the registers and scope
        mu_t regs[code->regs];
        regs[0] = scope;
        mu_fcopy(code->args, &regs[1], frame);

        // Setup other state
        imms = code_imms(code);
        fns = code_fns(code);
        pc = code_bcode(code);

        // Enter main execution loop
        VM_DISPATCH(pc)
            VM_ENTRY_DI(OP_IMM, d, i)
                regs[d] = mu_inc(imms[i]);
            VM_ENTRY_END

            VM_ENTRY_DI(OP_FN, d, i)
                regs[d] = fn_create(code_inc(fns[i]), mu_inc(regs[0]));
            VM_ENTRY_END

            VM_ENTRY_DI(OP_TBL, d, i)
                regs[d] = tbl_create(i);
            VM_ENTRY_END

            VM_ENTRY_DA(OP_MOVE, d, a)
                regs[d] = regs[a];
            VM_ENTRY_END

            VM_ENTRY_DA(OP_DUP, d, a)
                regs[d] = mu_inc(regs[a]);
            VM_ENTRY_END

            VM_ENTRY_D(OP_DROP, d)
                mu_dec(regs[d]);
            VM_ENTRY_END

            VM_ENTRY_DAB(OP_LOOKUP, d, a, b)
                regs[d] = mu_lookup(regs[a], regs[b]);
            VM_ENTRY_END

            VM_ENTRY_DAB(OP_LOOKDN, d, a, b)
                scratch = mu_lookup(regs[a], regs[b]);
                mu_dec(regs[a]);
                regs[d] = scratch;
            VM_ENTRY_END

            VM_ENTRY_DAB(OP_INSERT, d, a, b)
                mu_insert(regs[a], regs[b], regs[d]);
            VM_ENTRY_END

            VM_ENTRY_DAB(OP_ASSIGN, d, a, b)
                mu_assign(regs[a], regs[b], regs[d]);
            VM_ENTRY_END

            VM_ENTRY_DJ(OP_JUMP, d, j)
                pc += j;
            VM_ENTRY_END

            VM_ENTRY_DJ(OP_JTRUE, d, j)
                if (regs[d])
                    pc += j;
            VM_ENTRY_END

            VM_ENTRY_DJ(OP_JFALSE, d, j)
                if (!regs[d])
                    pc += j;
            VM_ENTRY_END

            VM_ENTRY_DA(OP_CALL, d, a)
                mu_fcopy(a >> 4, frame, &regs[d+1]);
                mu_fcall(regs[d], a, frame);
                mu_dec(regs[d]);
                mu_fcopy(0xf & a, &regs[d], frame);
            VM_ENTRY_END

            VM_ENTRY_DA(OP_RET, d, a)
                mu_fcopy(a, frame, &regs[d]);
                tbl_dec(scope);
                code_dec(code);
                return a;
            VM_ENTRY_END

            VM_ENTRY_DA(OP_TCALL, d, a)
                scratch = regs[d];
                mu_fcopy(a, frame, &regs[d+1]);
                tbl_dec(scope);
                code_dec(code);

                // Use a direct goto to garuntee a tail call when the target
                // is another mu function. Otherwise, we just try our hardest
                // to get a tail call emitted.
                if (mu_type(scratch) == MTFN && fn_type(scratch) == FTMFN) {
                    code = fn_code(scratch);
                    mu_fconvert(code->args, a, frame);
                    scope = tbl_extend(code->scope, fn_closure(scratch));
                    fn_dec(scratch);
                    goto reenter;
                } else {
                    return mu_tcall(scratch, a, frame);
                }
            VM_ENTRY_END
        VM_DISPATCH_END
    }
}

