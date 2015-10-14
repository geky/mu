#include "vm.h"

#include "mu.h"
#include "parse.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"


// Instruction access functions
mu_inline enum op  op(const mbyte_t *pc) { return 0xf & pc[0] >> 4; }
mu_inline unsigned d(const mbyte_t *pc)  { return 0xf & pc[0] >> 0; }
mu_inline unsigned a(const mbyte_t *pc)  { return 0xf & pc[1] >> 4; }
mu_inline unsigned b(const mbyte_t *pc)  { return 0xf & pc[1] >> 0; }
mu_inline unsigned c(const mbyte_t *pc)  { return pc[1]; }
mu_inline unsigned i(const mbyte_t *pc)  { return (pc[2] << 8) | pc[1]; }
mu_inline signed   j(const mbyte_t *pc)  { return (int16_t)i(pc); }

// Bytecode errors
static mu_noreturn mu_error_bytecode(void) {
    mu_error(mcstr("exceeded bytecode limits"));
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
    } else if (op >= OP_IMM && op <= OP_TBL) {
        if (a > 0xffff)
            mu_error_bytecode();

        emit(p, a);
        emit(p, a >> 8);
    } else if (op >= OP_LOOKDN && op <= OP_ASSIGN) {
        if (a > 0xf || b > 0xf)
            mu_error_bytecode();

        emit(p, (a << 4) | b);
    } else if (op >= OP_JFALSE && op <= OP_JUMP) {
        a -= 3;
        if (a > 0x7fff || a < -0x8000)
            mu_error_bytecode();

        emit(p, a);
        emit(p, a >> 8);
    }
}

mint_t mu_patch(void *c, mint_t nj) {
    mbyte_t *p = c;
    int16_t pj = p[1] | (p[2] << 8);

    mu_assert((p[0] >> 4) >= OP_JFALSE && (p[0] >> 4) <= OP_JUMP);

    nj -= 3;
    if (nj > 0x7fff || nj < -0x8000)
        mu_error_bytecode();

    p[1] = nj;
    p[2] = nj >> 8;

    return pj + 3;
}


// Disassemble bytecode for debugging and introspection
// currently just outputs to stdout
// Unsure if this should be kept as is, returned as string
// or just dropped completely.
// TODO change this to Mu prints?
void mu_dis(struct code *code) {
    mu_t *imms = code_imms(code);
    struct code **fns = code_fns(code);
    const mbyte_t *pc = code_bcode(code);
    const mbyte_t *end = pc + code->bcount;

#ifdef MU32
#define PTR "%08x"
#else
#define PTR "%016lx"
#endif

    printf("-- dis 0x"PTR" --\n", (muint_t)code);
    printf("regs: %u, scope: %u, args: %x\n",
           code->regs, code->scope, code->args);

    if (code->icount > 0) {
        printf("imms:\n");
        for (muint_t i = 0; i < code->icount; i++) {
            mu_t repr = mu_repr(mu_inc(imms[i]));
            printf(PTR" (%.*s)\n", (muint_t)imms[i], str_len(repr), str_bytes(repr));
            str_dec(repr);
        }
    }

    if (code->fcount > 0) {
        printf("fns:\n");
        for (muint_t i = 0; i < code->fcount; i++) {
            printf(PTR"\n", (muint_t)fns[i]);
        }
    }

    printf("bcode:\n");
    while (pc < end) {

        switch (op(pc)) {
            case OP_IMM:
                printf("%02x%02x%02x ", pc[0], pc[1], pc[2]);
                printf("imm r%d, %d", d(pc), i(pc));
                { mu_t repr = mu_repr(mu_inc(imms[i(pc)]));
                  printf("(%.*s)\n", str_len(repr), str_bytes(repr));
                  str_dec(repr);
                }
                pc += 3;
                break;
            case OP_FN:
                printf("%02x%02x%02x ", pc[0], pc[1], pc[2]);
                printf("fn r%d, %d\n", d(pc), i(pc));
                pc += 3;
                break;
            case OP_TBL:
                printf("%02x%02x%02x ", pc[0], pc[1], pc[2]);
                printf("tbl r%d, %d\n", d(pc), i(pc));
                pc += 3;
                break;
            case OP_MOVE:
                printf("%02x%02x   ", pc[0], pc[1]);
                printf("move r%d, r%d\n", d(pc), c(pc));
                pc += 2;
                break;
            case OP_DUP:
                printf("%02x%02x   ", pc[0], pc[1]);
                printf("dup r%d, r%d\n", d(pc), c(pc));
                pc += 2;
                break;
            case OP_DROP:
                printf("%02x     ", pc[0]);
                printf("drop r%d\n", d(pc));
                pc += 1;
                break;
            case OP_LOOKUP:
                printf("%02x%02x   ", pc[0], pc[1]);
                printf("lookup r%d, r%d[r%d]\n", d(pc), a(pc), b(pc));
                pc += 2;
                break;
            case OP_LOOKDN:
                printf("%02x%02x   ", pc[0], pc[1]);
                printf("lookdn r%d, r%d[r%d]\n", d(pc), a(pc), b(pc));
                pc += 2;
                break;
            case OP_INSERT:
                printf("%02x%02x   ", pc[0], pc[1]);
                printf("insert r%d, r%d[r%d]\n", d(pc), a(pc), b(pc));
                pc += 2;
                break;
            case OP_ASSIGN:
                printf("%02x%02x   ", pc[0], pc[1]);
                printf("assign r%d, r%d[r%d]\n", d(pc), a(pc), b(pc));
                pc += 2;
                break;
            case OP_JUMP:
                printf("%02x%02x%02x ", pc[0], pc[1], pc[2]);
                printf("jump %d\n", j(pc));
                pc += 3;
                break;
            case OP_JTRUE:
                printf("%02x%02x%02x ", pc[0], pc[1], pc[2]);
                printf("jtrue r%d, %d\n", d(pc), j(pc));
                pc += 3;
                break;
            case OP_JFALSE:
                printf("%02x%02x%02x ", pc[0], pc[1], pc[2]);
                printf("jfalse r%d, %d\n", d(pc), j(pc));
                pc += 3;
                break;
            case OP_CALL:
                printf("%02x%02x   ", pc[0], pc[1]);
                printf("call r%d, 0x%02x\n", d(pc), c(pc));
                pc += 2;
                break;
            case OP_TCALL:
                printf("%02x%02x   ", pc[0], pc[1]);
                printf("tcall r%d, 0x%01x\n", d(pc), c(pc));
                pc += 2;
                break;
            case OP_RET:
                printf("%02x%02x   ", pc[0], pc[1]);
                printf("ret r%d, 0x%01x\n", d(pc), c(pc));
                pc += 2;
                break;
        }
    }
}


// Execute bytecode
mc_t mu_exec(struct code *code, mu_t scope, mu_t *frame) {
    // Allocate temporary variables
    register const mbyte_t *pc;
    mu_t *imms;
    struct code **fns;

    register mu_t scratch;

reenter:
    {   mu_dis(code);

        // Setup the registers and scope
        mu_t regs[code->regs];
        regs[0] = scope;
        mu_fcopy(code->args, &regs[1], frame);

        // Setup other state
        imms = code_imms(code);
        fns = code_fns(code);
        pc = code_bcode(code);

        // Enter main execution loop
        while (1) {
            switch (op(pc)) {
                case OP_IMM:
                    regs[d(pc)] = mu_inc(imms[i(pc)]);
                    pc += 3;
                    break;

                case OP_FN:
                    regs[d(pc)] = mcode(code_inc(fns[i(pc)]), mu_inc(regs[0]));
                    pc += 3;
                    break;

                case OP_TBL:
                    regs[d(pc)] = tbl_create(i(pc));
                    pc += 3;
                    break;

                case OP_MOVE:
                    regs[d(pc)] = regs[c(pc)];
                    pc += 2;
                    break;

                case OP_DUP:
                    regs[d(pc)] = mu_inc(regs[c(pc)]);
                    pc += 2;
                    break;

                case OP_DROP:
                    mu_dec(regs[d(pc)]);
                    pc += 1;
                    break;

                case OP_LOOKUP:
                    scratch = mu_lookup(regs[a(pc)], regs[b(pc)]);
                    regs[d(pc)] = scratch;
                    pc += 2;
                    break;

                case OP_LOOKDN:
                    scratch = mu_lookup(regs[a(pc)], regs[b(pc)]);
                    mu_dec(regs[a(pc)]);
                    regs[d(pc)] = scratch;
                    pc += 2;
                    break;

                case OP_INSERT:
                    mu_insert(regs[a(pc)], regs[b(pc)], regs[d(pc)]);
                    pc += 2;
                    break;

                case OP_ASSIGN:
                    mu_assign(regs[a(pc)], regs[b(pc)], regs[d(pc)]);
                    pc += 2;
                    break;

                case OP_JUMP:
                    pc += j(pc);
                    pc += 3;
                    break;

                case OP_JTRUE:
                    if (regs[d(pc)])
                        pc += j(pc);
                    pc += 3;
                    break;

                case OP_JFALSE:
                    if (!regs[d(pc)])
                        pc += j(pc);
                    pc += 3;
                    break;

                case OP_CALL:
                    mu_fcopy(c(pc) >> 4, frame, &regs[d(pc)+1]);
                    mu_fcall(regs[d(pc)], c(pc), frame);
                    mu_dec(regs[d(pc)]);
                    mu_fcopy(0xf & c(pc), &regs[d(pc)], frame);
                    pc += 2;
                    break;

                case OP_RET:
                    mu_fcopy(c(pc), frame, &regs[d(pc)]);
                    tbl_dec(scope);
                    code_dec(code);
                    return c(pc);

                case OP_TCALL:
                    scratch = regs[d(pc)];
                    mu_fcopy(c(pc), frame, &regs[d(pc)+1]);
                    tbl_dec(scope);
                    code_dec(code);

                    // Use a direct goto to garuntee a tail call when the target
                    // is another mu function. Otherwise, we just try our hardest
                    // to get a tail call emitted.
                    if (mu_type(scratch) == MTFN) {
                        code = fn_code(scratch);
                        mu_fconvert(code->args, c(pc), frame);
                        scope = tbl_extend(code->scope, fn_closure(scratch));
                        fn_dec(scratch);
                        goto reenter;
                    } else {
                        return mu_tcall(scratch, c(pc), frame);
                    }
            }
        }
    }

    mu_unreachable;
}

