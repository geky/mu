#include "vm.h"

#include "mu.h"
#include "parse.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"


// Bytecode errors
static mu_noreturn mu_error_bytecode(void) {
    mu_errorf("exceeded bytecode limits");
}


// Encode the specified opcode and return its size
// Encode the specified opcode and return its size
// Note: size of the jump opcodes currently can not change based on argument
void mu_encode(void (*emit)(void *, mbyte_t), void *p,
               enum mop op, mint_t d, mint_t a, mint_t b) {
    union {
        uint16_t u16;
        uint8_t u8[2];
    } ins;

    if (op > 0xf || d > 0xf) {
        mu_error_bytecode();
    }

    ins.u16 =  0xf000 & (op << 12);
    ins.u16 |= 0x0f00 & (d << 8);

    if (op >= MOP_RET && op <= MOP_DROP) {
        if (a > 0xff) {
            mu_error_bytecode();
        }

        ins.u16 |= 0x00ff & a;
        emit(p, ins.u8[0]);
        emit(p, ins.u8[1]);
    } else if (op >= MOP_LOOKDN && op <= MOP_ASSIGN) {
        if (a > 0xf || b > 0xf) {
            mu_error_bytecode();
        }

        ins.u16 |= 0x00f0 & (a << 4);
        ins.u16 |= 0x000f & b;
        emit(p, ins.u8[0]);
        emit(p, ins.u8[1]);
    } else if (op >= MOP_IMM && op <= MOP_TBL) {
        if (a > 0xffff) {
            mu_error_bytecode();
        } else if (a > 0xfe) {
            ins.u16 |= 0x00ff;
            emit(p, ins.u8[0]);
            emit(p, ins.u8[1]);

            ins.u16 = a;
            emit(p, ins.u8[0]);
            emit(p, ins.u8[1]);
        } else {
            ins.u16 |= 0x00ff & a;
            emit(p, ins.u8[0]);
            emit(p, ins.u8[1]);
        }
    } else if (op >= MOP_JFALSE && op <= MOP_JUMP) {
        a = (a / 2) - 2;

        if (a > 0x7fff || a < -0x8000) {
            mu_error_bytecode();
        }

        ins.u16 |= 0x00ff;
        emit(p, ins.u8[0]);
        emit(p, ins.u8[1]);

        ins.u16 = a;
        emit(p, ins.u8[0]);
        emit(p, ins.u8[1]);
    }
}

mint_t mu_patch(void *p, mint_t nj) {
    uint16_t *c = p;
    mu_assert((c[0] >> 12) >= MOP_JFALSE && (c[0] >> 12) <= MOP_JUMP);

    mint_t pj = c[1];
    c[1] = (nj / 2) - 2;

    return pj;
}


// Disassemble bytecode for debugging and introspection
// currently outputs to stdout
static const char *const op_names[16] = {
    [MOP_IMM]    = "imm",
    [MOP_FN]     = "fn",
    [MOP_TBL]    = "tbl",
    [MOP_MOVE]   = "move",
    [MOP_DUP]    = "dup",
    [MOP_DROP]   = "drop",
    [MOP_LOOKUP] = "lookup",
    [MOP_LOOKDN] = "lookdn",
    [MOP_INSERT] = "insert",
    [MOP_ASSIGN] = "assign",
    [MOP_JUMP]   = "jump",
    [MOP_JTRUE]  = "jtrue",
    [MOP_JFALSE] = "jfalse",
    [MOP_CALL]   = "call",
    [MOP_TCALL]  = "tcall",
    [MOP_RET]    = "ret",
};

void mu_dis(mu_t c) {
    mu_t *imms = mu_code_getimms(c);
    const uint16_t *pc = mu_code_getbcode(c);
    const uint16_t *end = pc + mu_code_getbcodelen(c)/2;

    mu_printf("-- dis 0x%wx --", c);
    mu_printf("regs: %qu, scope: %qu, args: 0x%bx",
            mu_code_getheader(c)->regs,
            mu_code_getheader(c)->scope,
            mu_code_getheader(c)->args);

    if (mu_code_getimmslen(c) > 0) {
        mu_printf("imms:");
        for (muint_t i = 0; i < mu_code_getimmslen(c); i++) {
            mu_printf("%wx (%r)", imms[i], mu_inc(imms[i]));
        }
    }

    mu_printf("bcode:");
    while (pc < end) {
        enum mop op = pc[0] >> 12;

        if (op == MOP_DROP) {
            mu_printf("%bx%bx      %s r%d",
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8));
            pc += 1;
        } else if (op >= MOP_RET && op <= MOP_DROP) {
            mu_printf("%bx%bx      %s r%d, 0x%bx",
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), 0xff & pc[0]);
            pc += 1;
        } else if (op >= MOP_LOOKDN && op <= MOP_ASSIGN) {
            mu_printf("%bx%bx      %s r%d, r%d[r%d]",
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), 0xf & (pc[0] >> 4), 0xf & pc[0]);
            pc += 1;
        } else if (op == MOP_IMM && (0xff & pc[0]) == 0xff) {
            mu_printf("%bx%bx%bx%bx  %s r%d, %u (%r)",
                    pc[0] >> 8, 0xff & pc[0],
                    pc[1] >> 8, 0xff & pc[1], op_names[op],
                    0xf & (pc[0] >> 8), pc[1],
                    mu_inc(imms[pc[1]]));
            pc += 2;
        } else if (op == MOP_IMM) {
            mu_printf("%bx%bx      %s r%d, %u (%r)",
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), 0x7f & pc[0],
                    mu_inc(imms[0x7f & pc[0]]));
            pc += 1;
        } else if (op >= MOP_IMM && op <= MOP_TBL && (0xff & pc[0]) == 0xff) {
            mu_printf("%bx%bx%bx%bx  %s r%d, %u",
                    pc[0] >> 8, 0xff & pc[0],
                    pc[1] >> 8, 0xff & pc[1], op_names[op],
                    0xf & (pc[0] >> 8), pc[1]);
            pc += 2;
        } else if (op >= MOP_IMM && op <= MOP_TBL) {
            mu_printf("%bx%bx      %s r%d, %u",
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), 0x7f & pc[0]);
            pc += 1;
        } else if (op >= MOP_JFALSE && op <= MOP_JUMP && (0xff & pc[0]) == 0xff) {
            mu_printf("%bx%bx%bx%bx  %s r%d, %d",
                    pc[0] >> 8, 0xff & pc[0],
                    pc[1] >> 8, 0xff & pc[1], op_names[op],
                    0xf & (pc[0] >> 8), (int16_t)pc[1]);
            pc += 2;
        } else if (op >= MOP_JFALSE && op <= MOP_JUMP) {
            mu_printf("%bx%bx      %s r%d, %d",
                    pc[0] >> 8, 0xff & pc[0], op_names[op],
                    0xf & (pc[0] >> 8), (int16_t)(pc[0] << 8) >> 8);
            pc += 1;
        }
    }
}


// Virtual machine dispatch macros
#ifdef MU_COMPUTED_GOTO
#define VM_DISPATCH(pc)                                                     \
    {   static void *const vm_entry[16] = {                                 \
            [MOP_IMM]    = &&VM_ENTRY_MOP_IMM,                              \
            [MOP_FN]     = &&VM_ENTRY_MOP_FN,                               \
            [MOP_TBL]    = &&VM_ENTRY_MOP_TBL,                              \
            [MOP_MOVE]   = &&VM_ENTRY_MOP_MOVE,                             \
            [MOP_DUP]    = &&VM_ENTRY_MOP_DUP,                              \
            [MOP_DROP]   = &&VM_ENTRY_MOP_DROP,                             \
            [MOP_LOOKUP] = &&VM_ENTRY_MOP_LOOKUP,                           \
            [MOP_LOOKDN] = &&VM_ENTRY_MOP_LOOKDN,                           \
            [MOP_INSERT] = &&VM_ENTRY_MOP_INSERT,                           \
            [MOP_ASSIGN] = &&VM_ENTRY_MOP_ASSIGN,                           \
            [MOP_JUMP]   = &&VM_ENTRY_MOP_JUMP,                             \
            [MOP_JTRUE]  = &&VM_ENTRY_MOP_JTRUE,                            \
            [MOP_JFALSE] = &&VM_ENTRY_MOP_JFALSE,                           \
            [MOP_CALL]   = &&VM_ENTRY_MOP_CALL,                             \
            [MOP_TCALL]  = &&VM_ENTRY_MOP_TCALL,                            \
            [MOP_RET]    = &&VM_ENTRY_MOP_RET,                              \
        };                                                                  \
                                                                            \
        while (1) {                                                         \
            register uint16_t ins = *pc++;                                  \
            goto *vm_entry[ins >> 12];
#define VM_DISPATCH_END                                                     \
        }                                                                   \
        mu_unreachable;                                                     \
    }

#define VM_ENTRY(op)                                                        \
    VM_ENTRY_##op: {
#define VM_ENTRY_END                                                        \
        goto *vm_entry[*pc >> 4];                                           \
    }
#else
#define VM_DISPATCH(pc)                                                     \
    {                                                                       \
        while (1) {                                                         \
            register uint16_t ins = *pc++;                                  \
            switch (ins >> 12) {
#define VM_DISPATCH_END                                                     \
            }                                                               \
        }                                                                   \
        mu_unreachable;                                                     \
    }

#define VM_ENTRY(op)                                                        \
    case op: {
#define VM_ENTRY_END                                                        \
        break;                                                              \
    }
#endif


#define VM_ENTRY_DA(op, d, a)                                               \
    VM_ENTRY(op)                                                            \
        mu_unused unsigned d = 0xf & (ins >> 8);                            \
        mu_unused unsigned a = 0xff & ins;

#define VM_ENTRY_DAB(op, d, a, b)                                           \
    VM_ENTRY(op)                                                            \
        mu_unused unsigned d = 0xf & (ins >> 8);                            \
        mu_unused unsigned a = 0xf & (ins >> 4);                            \
        mu_unused unsigned b = 0xf & (ins >> 0);

#define VM_ENTRY_DI(op, d, i)                                               \
    VM_ENTRY(op)                                                            \
        mu_unused unsigned d = 0xf & (ins >> 8);                            \
        mu_unused muint_t i = 0xff & ins;                                   \
        if (i == 0xff) {                                                    \
            i = *pc++;                                                      \
        }

#define VM_ENTRY_DJ(op, d, j)                                               \
    VM_ENTRY(op)                                                            \
        mu_unused unsigned d = 0xf & (ins >> 8);                            \
        mu_unused mint_t j = (int16_t)(ins << 8) >> 8;                      \
        if (j == -1) {                                                      \
            j = (int16_t)*pc++;                                             \
        }



mcnt_t mu_exec(mu_t c, mu_t scope, mu_t *frame) {
    mu_assert(mu_iscode(c));

    // Allocate temporary variables
    const uint16_t *pc;
    mu_t *imms;

#ifdef MU_DISASSEMBLE
    mu_dis(c);
#endif

reenter:
    {   // Setup the registers and scope
        mu_t regs[mu_code_getheader(c)->regs];
        regs[0] = scope;
        mu_frame_move(mu_code_getheader(c)->args, &regs[1], frame);

        // Setup other state
        imms = mu_code_getimms(c);
        pc = mu_code_getbcode(c);

        // Enter main execution loop
        VM_DISPATCH(pc)
            VM_ENTRY_DI(MOP_IMM, d, i)
                regs[d] = mu_inc(imms[i]);
            VM_ENTRY_END

            VM_ENTRY_DI(MOP_FN, d, i)
                regs[d] = mu_fn_fromcode(mu_code_inc(imms[i]), mu_inc(regs[0]));
            VM_ENTRY_END

            VM_ENTRY_DI(MOP_TBL, d, i)
                regs[d] = mu_tbl_create(i);
            VM_ENTRY_END

            VM_ENTRY_DA(MOP_MOVE, d, a)
                regs[d] = regs[a];
            VM_ENTRY_END

            VM_ENTRY_DA(MOP_DUP, d, a)
                regs[d] = mu_inc(regs[a]);
            VM_ENTRY_END

            VM_ENTRY_DA(MOP_DROP, d, a)
                mu_dec(regs[d]);
            VM_ENTRY_END

            VM_ENTRY_DAB(MOP_LOOKUP, d, a, b)
                if (!mu_istbl(regs[a])) {
                    mu_errorf("unable to lookup %r in %r", regs[b], regs[a]);
                }

                regs[d] = mu_tbl_lookup(regs[a], regs[b]);
            VM_ENTRY_END

            VM_ENTRY_DAB(MOP_LOOKDN, d, a, b)
                if (!mu_istbl(regs[a])) {
                    mu_errorf("unable to lookup %r in %r", regs[b], regs[a]);
                }

                mu_t scratch = mu_tbl_lookup(regs[a], regs[b]);
                mu_dec(regs[a]);
                regs[d] = scratch;
            VM_ENTRY_END

            VM_ENTRY_DAB(MOP_INSERT, d, a, b)
                if (!mu_istbl(regs[a])) {
                    mu_errorf("unable to insert %r to %r in %r",
                            regs[d], regs[b], regs[a]);
                }

                mu_tbl_insert(regs[a], regs[b], regs[d]);
            VM_ENTRY_END

            VM_ENTRY_DAB(MOP_ASSIGN, d, a, b)
                if (!mu_istbl(regs[a])) {
                    mu_errorf("unable to assign %r to %r in %r",
                            regs[d], regs[b], regs[a]);
                }

                mu_tbl_assign(regs[a], regs[b], regs[d]);
            VM_ENTRY_END

            VM_ENTRY_DJ(MOP_JUMP, d, j)
                pc += j;
            VM_ENTRY_END

            VM_ENTRY_DJ(MOP_JTRUE, d, j)
                if (regs[d]) {
                    pc += j;
                }
            VM_ENTRY_END

            VM_ENTRY_DJ(MOP_JFALSE, d, j)
                if (!regs[d]) {
                    pc += j;
                }
            VM_ENTRY_END

            VM_ENTRY_DA(MOP_CALL, d, a)
                if (!mu_isfn(regs[d])) {
                    mu_errorf("unable to call %r", regs[d]);
                }

                mu_frame_move(a >> 4, frame, &regs[d+1]);
                mu_fn_fcall(regs[d], a, frame);
                mu_dec(regs[d]);
                mu_frame_move(0xf & a, &regs[d], frame);
            VM_ENTRY_END

            VM_ENTRY_DA(MOP_RET, d, a)
                mu_frame_move(a, frame, &regs[d]);
                mu_tbl_dec(scope);
                mu_code_dec(c);
                return a;
            VM_ENTRY_END

            VM_ENTRY_DA(MOP_TCALL, d, a)
                mu_t scratch = regs[d];
                mu_frame_move(a, frame, &regs[d+1]);
                mu_tbl_dec(scope);
                mu_code_dec(c);

                // Use a direct goto to garuntee a tail call when the target
                // is another mu function. Otherwise, we just try our hardest
                // to get a tail call emitted.
                if (!mu_isfn(scratch)) {
                    mu_errorf("unable to call %r", scratch);
                }

                c = mu_fn_getcode(scratch);
                if (c) {
                    mu_frame_convert(a, mu_code_getheader(c)->args, frame);
                    scope = mu_tbl_extend(mu_code_getheader(c)->scope,
                                mu_fn_getclosure(scratch));
                    mu_fn_dec(scratch);
                    goto reenter;
                } else {
                    return mu_fn_tcall(scratch, a, frame);
                }
            VM_ENTRY_END
        VM_DISPATCH_END
    }
}

