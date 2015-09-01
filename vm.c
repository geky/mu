#include "vm.h"

#include "parse.h"
#include "mu.h"
#include "num.h"
#include "str.h"
#include "fn.h"
#include "tbl.h"
#include "err.h"
#include <string.h>


// Instruction access functions
mu_inline enum op      op(uint16_t ins) { return (enum op)(ins >> 12); }
mu_inline unsigned int rd(uint16_t ins) { return 0xf & (ins >> 8); }
mu_inline unsigned int ra(uint16_t ins) { return 0xf & (ins >> 4); }
mu_inline unsigned int rb(uint16_t ins) { return 0xf & (ins >> 0); }
mu_inline unsigned int i(uint16_t ins)  { return (uint8_t)ins; }
mu_inline signed int   j(uint16_t ins)  { return (int8_t)ins; }


// Encode the specified opcode and return its size
// Encode the specified opcode and return its size
// Note: size of the jump opcodes currently can not change based on argument
// TODO: change asserts to err throwing checks
void mu_encode(void (*emit)(void *, mbyte_t), void *p,
               enum op op, mint_t d, mint_t a, mint_t b) {
    uint16_t ins = 0;
    mu_assert(op <= 0xf);
    mu_assert(d <= 0xf);
    ins |= op << 12;
    ins |= d << 8;

    if (op >= OP_RET && op <= OP_DROP) {
        mu_assert(a <= 0xff);
        ins |= a;
    } else if (op >= OP_JFALSE && op <= OP_JUMP) {
        mu_assert(a-2 <= 0xff && a-2 >= -0x100);
        ins |= 0xff & ((a-2)>>1);
    } else {
        mu_assert(a <= 0xf);
        mu_assert(b <= 0xf);
        ins |= a << 4;
        ins |= b;
    }

    emit(p, ((mbyte_t *)&ins)[0]);
    emit(p, ((mbyte_t *)&ins)[1]);
}

mint_t mu_patch(void *c, mint_t nj) {
    uint16_t ins = *(uint16_t *)c;
    mu_assert(op(ins) >= OP_JFALSE && op(ins) <= OP_JUMP);
    mu_assert(nj-2 <= 0xff && nj-2 >= -0x100);

    mint_t pj = (j(ins) << 1)+2;
    ins = (ins & 0xff00) | (0xff & ((nj-2) >> 1));
    *(uint16_t *)c = ins;

    return pj;
}


// Disassemble bytecode for debugging and introspection
// currently just outputs to stdout
// Unsure if this should be kept as is, returned as string
// or just dropped completely.
void mu_dis(struct code *c) {
    mu_t *imms = code_imms(c);
    struct code **fns = code_fns(c);
    const uint16_t *pc = code_bcode(c);
    const uint16_t *end = pc + c->bcount/2;
    uint16_t ins;

#ifdef MU32
#define PTR "%08x"
#else
#define PTR "%016lx"
#endif

    printf("-- dis 0x"PTR" --\n", (muint_t)c);
    printf("regs: %u, scope: %u, args: %x\n",
           c->regs, c->scope, c->args);

    if (c->icount > 0) {
        printf("imms:\n");
        for (muint_t i = 0; i < c->icount; i++) {
            mu_t repr = mu_repr(mu_inc(imms[i]));
            printf(PTR" (%.*s)\n", (muint_t)imms[i], str_len(repr), str_bytes(repr));
            str_dec(repr);
        }
    }

    if (c->fcount > 0) {
        printf("fns:\n");
        for (muint_t i = 0; i < c->fcount; i++) {
            printf(PTR"\n", (muint_t)fns[i]);
        }
    }

    printf("bcode:\n");
    while (pc < end) {
        ins = *pc++;
        printf("%04x ", ins);

        switch (op(ins)) {
            case OP_IMM:
                printf("imm r%d, %d", rd(ins), i(ins));
                { mu_t repr = mu_repr(mu_inc(imms[i(ins)]));
                  printf("(%.*s)\n", str_len(repr), str_bytes(repr));
                  str_dec(repr);
                }
                break;
            case OP_FN:
                printf("fn r%d, %d\n", rd(ins), i(ins));
                break;
            case OP_TBL:
                printf("tbl r%d, %d\n", rd(ins), i(ins));
                break;
            case OP_MOVE:
                printf("move r%d, r%d\n", rd(ins), i(ins));
                break;
            case OP_DUP:
                printf("dup r%d, r%d\n", rd(ins), i(ins));
                break;
            case OP_DROP:
                printf("drop r%d\n", rd(ins));
                break;
            case OP_LOOKUP:
                printf("lookup r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_LOOKDN:
                printf("lookdn r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_INSERT:
                printf("insert r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_ASSIGN:
                printf("assign r%d, r%d[r%d]\n", rd(ins), ra(ins), rb(ins));
                break;
            case OP_JUMP:
                printf("jump %d\n", j(ins));
                break;
            case OP_JTRUE:
                printf("jtrue r%d, %d\n", rd(ins), j(ins));
                break;
            case OP_JFALSE:
                printf("jfalse r%d, %d\n", rd(ins), j(ins));
                break;
            case OP_CALL:
                printf("call r%d, 0x%02x\n", rd(ins), i(ins));
                break;
            case OP_TCALL:
                printf("tcall r%d, 0x%01x\n", rd(ins), i(ins));
                break;
            case OP_RET:
                printf("ret r%d, 0x%01x\n", rd(ins), i(ins));
                break;
        }
    }
}


// Execute bytecode
mc_t mu_exec(struct code *c, mu_t scope, mu_t *frame) {
    // Allocate temporary variables
    register const uint16_t *pc;
    mu_t *imms;
    struct code **fns;

    register mu_t scratch;

reenter:
    // TODO Just here for debugging
#ifdef MU_DEBUG
    mu_dis(c);
#endif
    //

    {   // Setup the registers and scope
        mu_t regs[c->regs];
        regs[0] = scope;
        memcpy(&regs[1], frame, mu_fsize(c->args));

        // Setup other state
        imms = code_imms(c);
        fns = code_fns(c);
        pc = code_bcode(c);

        // Enter main execution loop
        while (1) {
            register uint16_t ins = *pc++;

            switch (op(ins)) {
                case OP_IMM:
                    regs[rd(ins)] = mu_inc(imms[i(ins)]);
                    break;

                case OP_FN:
                    regs[rd(ins)] = mfn(code_inc(fns[i(ins)]), mu_inc(regs[0]));
                    break;

                case OP_TBL:
                    regs[rd(ins)] = tbl_create(i(ins), 0);
                    break;

                case OP_MOVE:
                    regs[rd(ins)] = regs[i(ins)];
                    break;

                case OP_DUP:
                    regs[rd(ins)] = mu_inc(regs[i(ins)]);
                    break;

                case OP_DROP:
                    mu_dec(regs[rd(ins)]);
                    break;

                case OP_LOOKUP:
                    scratch = mu_lookup(regs[ra(ins)], regs[rb(ins)]);
                    regs[rd(ins)] = scratch;
                    break;

                case OP_LOOKDN:
                    scratch = mu_lookup(regs[ra(ins)], regs[rb(ins)]);
                    mu_dec(regs[ra(ins)]);
                    regs[rd(ins)] = scratch;
                    break;

                case OP_INSERT:
                    mu_insert(regs[ra(ins)], regs[rb(ins)], regs[rd(ins)]);
                    break;

                case OP_ASSIGN:
                    mu_assign(regs[ra(ins)], regs[rb(ins)], regs[rd(ins)]);
                    break;

                case OP_JUMP:
                    pc += j(ins);
                    break;

                case OP_JTRUE:
                    if (regs[rd(ins)])
                        pc += j(ins);
                    break;

                case OP_JFALSE:
                    if (!regs[rd(ins)])
                        pc += j(ins);
                    break;

                case OP_CALL:
                    memcpy(frame, &regs[rd(ins)+1], mu_fsize(i(ins) >> 4));
                    mu_fcall(regs[rd(ins)], i(ins), frame);
                    memcpy(&regs[rd(ins)], frame, mu_fsize(0xf & i(ins)));
                    break;

                case OP_RET:
                    memcpy(frame, &regs[rd(ins)], mu_fsize(i(ins)));
                    tbl_dec(scope);
                    code_dec(c);
                    return i(ins);

                case OP_TCALL:
                    scratch = regs[rd(ins)];
                    memcpy(frame, &regs[rd(ins)+1], mu_fsize(i(ins)));
                    tbl_dec(scope);
                    code_dec(c);

                    // Use a direct goto to garuntee a tail call when the target
                    // is another mu function. Otherwise, we just try our hardest
                    // to get a tail call emitted.
                    if (mu_type(scratch) == MU_FN) {
                        c = fn_code(scratch);
                        scope = tbl_create(c->scope, fn_closure(scratch));
                        mu_fto(c->args, i(ins), frame);
                        fn_dec(scratch);
                        goto reenter;
                    } else {
                        return mu_tcall(scratch, i(ins), frame);
                    }

                default:
                    mu_cerr(mcstr("invalid opcode"),
                            mcstr("invalid opcode"));
            }
        }
    }

    mu_unreachable;
}

