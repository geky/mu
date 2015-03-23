#include "vm.h"

#include "parse.h"
#include "types.h"
#include "num.h"
#include "str.h"
#include "fn.h"
#include "tbl.h"

#include <string.h>


// Conversion function for handling frames
void mu_fconvert(c_t sc, mu_t *sframe, c_t dc, mu_t *dframe) {
    if (dc > MU_FRAME) {
        if (sc > MU_FRAME) {
            *dframe = *sframe;
        } else {
            tbl_t *tbl = tbl_create(sc);

            for (uint_t i = 0; i < sc; i++)
                tbl_insert(tbl, muint(i), sframe[i]);

            *dframe = mtbl(tbl);
        }
    } else {
        if (sc >= MU_FRAME) {
            tbl_t *tbl = gettbl(*sframe);

            for (uint_t i = 0; i < dc; i++)
                dframe[i] = tbl_lookup(tbl, muint(i));

            tbl_dec(tbl);
        } else {
            for (uint_t i = 0; i < sc && i < dc; i++)
                dframe[i] = sframe[i];

            for (uint_t i = dc; i < sc; i++)
                mu_dec(sframe[i]);

            for (uint_t i = sc; i < dc; i++)
                dframe[i] = mnil;
        }
    }
}


// Encode the specified opcode and return its size
// Encode the specified opcode and return its size
// Note: size of the jump opcodes currently can not change based on argument
void mu_encode(void (*emit)(void *, data_t), void *p,
               op_t op, uint_t arg) {
    if (op >= OP_JUMP) {
        emit(p, (0xf0 & (op << 3)) | (0x0f & (arg >> 8)));
        emit(p, arg);
    } else if (op >= OP_CALL) {
        emit(p, (0xf8 & (op << 3)));
        emit(p, arg);
    } else {
        emit(p, (0xf8 & (op << 3)) | (0x0f & arg));
    }
}


// bytecode does not need to be portable, as it
// is compiled ad-hoc, but it does need to worry
// about alignment issues.
mu_inline uint_t argi(const data_t *pc) {
    return ((0xf & pc[0]) << 8) | pc[1];
}

mu_inline int_t argj(const data_t *pc) {
    return (int16_t)((0xf & pc[0]) << 8) | pc[1];
}

mu_inline uint_t argf(const data_t *pc) {
    return 0x7 & *pc;
}

mu_inline uint_t argc(const data_t *pc) {
    return *(pc+1);
}


// Execute the bytecode
void mu_exec(fn_t *fn, c_t c, mu_t *frame) {
    // Allocate registers
    register const data_t *pc;
    register mu_t *sp;
    register tbl_t *scope;
    register mu_t scratch;
    mu_t *imms;
    struct code **fns;

reenter:
    {   // Setup the stack
        mu_t stack[fn->flags.stack];
        sp = stack + fn->flags.stack;

        mu_fconvert(mu_args(c), frame, fn->flags.args, stack);
        sp -= fn->flags.args > MU_FRAME ? 1 : fn->flags.args;

        // Setup other state
        imms = code_imms(fn->code);
        fns = code_fns(fn->code);
        pc = code_bcode(fn->code);
        scope = tbl_extend(fn->flags.scope, fn->closure);

        // Enter main execution loop
        while (1) {
            switch (*pc >> 3) {
                case OP_IMM:
                    sp[1] = mu_inc(imms[argi(pc)]);
                    sp += 1;
                    pc += 2;
                    break;

                case OP_SYM:
                    sp[1] = mu_inc(mtbl(scope));
                    sp[2] = mu_inc(imms[argi(pc)]);
                    sp += 2;
                    pc += 2;
                    break;

                case OP_FN:
                    sp[1] = mfn(fn_create(fns[argi(pc)], scope));
                    sp += 1;
                    pc += 2;
                    break;

                case OP_TBL:
                    sp[1] = mtbl(tbl_create(argi(pc)));
                    sp += 1;
                    pc += 2;
                    break;

                case OP_DUP:
                    // fallthroughs intentional
                    switch (argf(pc)) {
                        case 7: sp[argf(pc)-6] = mu_inc(sp[-6]);
                        case 6: sp[argf(pc)-5] = mu_inc(sp[-5]);
                        case 5: sp[argf(pc)-4] = mu_inc(sp[-4]);
                        case 4: sp[argf(pc)-3] = mu_inc(sp[-3]);
                        case 3: sp[argf(pc)-2] = mu_inc(sp[-2]);
                        case 2: sp[argf(pc)-1] = mu_inc(sp[-1]);
                        case 1: sp[argf(pc)-0] = mu_inc(sp[-0]);
                        case 0: break;
                    }
                    sp -= argf(pc);
                    pc += 1;
                    break;

                case OP_PAD:
                    // fallthroughs intentional
                    switch (argf(pc)) {
                        case 7: sp[7] = mnil;
                        case 6: sp[6] = mnil;
                        case 5: sp[5] = mnil;
                        case 4: sp[4] = mnil;
                        case 3: sp[3] = mnil;
                        case 2: sp[2] = mnil;
                        case 1: sp[1] = mnil;
                        case 0: break;
                    }
                    sp += argf(pc);
                    pc += 1;
                    break;

                case OP_DROP:
                    // fallthroughs intentional
                    switch (argf(pc)) {
                        case 7: mu_dec(sp[-6]);
                        case 6: mu_dec(sp[-5]);
                        case 5: mu_dec(sp[-4]);
                        case 4: mu_dec(sp[-3]);
                        case 3: mu_dec(sp[-2]);
                        case 2: mu_dec(sp[-1]);
                        case 1: mu_dec(sp[-0]);
                        case 0: break;
                    }
                    sp -= argf(pc);
                    pc += 1;
                    break;

                case OP_BIND:
                    scratch = mu_lookup(sp[-1], sp[0]);
                    // TODO bind with sp[-1]
                    mu_dec(sp[-1]);
                    mu_dec(sp[0]);
                    sp[-1] = scratch;
                    sp -= 1;
                    pc += 1;

                case OP_LOOKUP:
                    scratch = mu_lookup(sp[-1], sp[0]);
                    mu_dec(sp[-1]);
                    mu_dec(sp[0]);
                    sp[-1] = scratch;
                    sp -= 1;
                    pc += 1;
                    break;

                case OP_FLOOKUP:
                    scratch = mu_lookup(sp[-1], sp[0]);
                    mu_dec(sp[0]);
                    sp[0] = scratch;
                    pc += 1;
                    break;

                case OP_INSERT:
                    mu_insert(sp[-1], sp[0], sp[-2-argf(pc)]);
                    mu_dec(sp[-1]);
                    switch (argf(pc)) {
                        case 7: sp[-9] = sp[-8];
                        case 6: sp[-8] = sp[-7];
                        case 5: sp[-7] = sp[-6];
                        case 4: sp[-6] = sp[-5];
                        case 3: sp[-5] = sp[-4];
                        case 2: sp[-4] = sp[-3];
                        case 1: sp[-3] = sp[-2];
                        case 0: break;
                    }
                    sp -= 3;
                    pc += 1;
                    break;

                case OP_ASSIGN:
                    mu_assign(sp[-1], sp[0], sp[-2-argf(pc)]);
                    mu_dec(sp[-1]);
                    switch (argf(pc)) {
                        case 7: sp[-9] = sp[-8];
                        case 6: sp[-8] = sp[-7];
                        case 5: sp[-7] = sp[-6];
                        case 4: sp[-6] = sp[-5];
                        case 3: sp[-5] = sp[-4];
                        case 2: sp[-4] = sp[-3];
                        case 1: sp[-3] = sp[-2];
                        case 0: break;
                    }
                    sp -= 3;
                    pc += 1;
                    break;

                case OP_FINSERT:
                    mu_insert(sp[-2], sp[-1], sp[0]);
                    sp -= 2;
                    pc += 1;
                    break;

                case OP_FASSIGN:
                    mu_assign(sp[-2], sp[-1], sp[0]);
                    sp -= 2;
                    pc += 1;
                    break;

                case OP_JUMP:
                case OP_JUMP+1:
                    pc += 2 + argj(pc);
                    break;

                case OP_JTRUE:
                case OP_JTRUE+1:
                    if (!isnil(sp[0]))
                        pc += 2 + argj(pc);
                    else
                        pc += 2;
                    break;

                case OP_JFALSE:
                case OP_JFALSE+1:
                    if (isnil(sp[0]))
                        pc += 2 + argj(pc);
                    else
                        pc += 2;
                    break;

                case OP_CALL:
                    scratch = sp[0];
                    sp -= 1 + mu_args(argc(pc));
                    mu_fcall(scratch, argc(pc), sp);
                    sp += mu_rets(argc(pc));
                    pc += 2;
                    mu_dec(scratch);
                    break;

                case OP_TCALL:
                    scratch = sp[0];
                    c = mu_c(argf(pc), mu_rets(c));
                    sp -= 1 + argf(pc);
                    memcpy(frame, sp, argf(pc));
                    tbl_dec(scope);
                    fn_dec(fn);
                    goto tailcall;

                case OP_RET:
                    sp -= argf(pc);
                    tbl_dec(scope);
                    fn_dec(fn);
                    mu_fconvert(argf(pc), sp, mu_rets(c), frame);
                    return;

                default:
                    mu_cerr(str_cstr("invalid opcode"),
                            str_cstr("invalid opcode"));
            }
        }
    }

    mu_unreachable();

tailcall:
    // Use a direct goto to garuntee a tail call when the target is
    // another mu function. Otherwise, we just try our hardest
    // to get a tail call emitted.
    if (isfn(scratch) && getfn(scratch)->flags.type == 0) {
        fn = getfn(scratch);
        goto reenter;
    } else {
        return mu_fcall(scratch, c, frame);
    }
}

