/*
 * Mu stand-alone interpreter
 */

#include "mu.h"
#include "types.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "err.h"
#include "parse.h"
#include "vm.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>

#define PROMPT_A "\033[32m> \033[0m"
#define PROMPT_B "\033[32m. \033[0m"
#define OUTPUT_A ""

#define ERR_START "\033[31m"
#define ERR_END   "\033[0m"

#define BUFFER_SIZE (mlen_t)-1


static mu_t scope = 0;
static mu_t args = 0;

static bool do_interpret = false;
static bool do_stdin = false;
static bool do_default = true;


static void printvar(mu_t v) {
    if (!mu_isstr(v)) {
        v = mu_repr(v);
    }

    printf("%.*s", str_len(v), str_bytes(v));
}

static void printrepr(mu_t v) {
    printvar(mu_repr(v));
}

static void printerr(mu_t err) {
    mu_try_begin {
        printf("%s", ERR_START);
        printvar(tbl_lookup(err, mcstr("type")));
        printf(" error: ");
        printvar(tbl_lookup(err, mcstr("reason")));
        printf("%s\n", ERR_END);
    } mu_on_err (err) {
        printf("%serror handling error%s\n", ERR_START, ERR_END);
    } mu_try_end;
}

static void printoutput(mu_t f) {
    int i = tbl_len(f);

    if (i == 0)
        return;

    printf("%s", OUTPUT_A);
//    tbl_for_begin (k, v, f) {
//        if (!mu_isnum(k)) {
//            printrepr(k);
//            printf(": ");
//        }
//
//        printrepr(v);
//
//        if (--i != 0) {
//            printf(", ");
//        }
//    } tbl_for_end

    mu_t k, v;
    for (muint_t j = 0; tbl_next(f, &j, &k, &v);) {
        if (!mu_isnum(k)) {
            printrepr(k);
            printf(": ");
        }

        printrepr(v);

        if (--i != 0) {
            printf(", ");
        }
    }
    printf("\n");
}

static mlen_t prompt(mbyte_t *input) {
    printf("%s", PROMPT_A);
    mlen_t len = 0;

    while (1) {
        mbyte_t c = getchar();
        input[len++] = c;

        if (c == '\n')
            return len;
    }
}


// TODO move this scope declaration somewhere else
static mc_t b_add(mu_t *frame) {
    if (mu_isnum(frame[0]) && mu_isnum(frame[1])) {
        frame[0] = num_add(frame[0], frame[1]);
    } else if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_add(frame[0], frame[1]);
    } else {
        assert(!!!"incompatible types");
    }
        
    return 1;
}

static mc_t b_mul(mu_t *frame) {
    if (mu_isnum(frame[0]) && mu_isnum(frame[1])) {
        frame[0] = num_mul(frame[0], frame[1]);
    } else if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_mul(frame[0], frame[1]);
    } else {
        assert(!!!"incompatible types");
    }
        
    return 1;
}

static mc_t b_div(mu_t *frame) {
    if (mu_isnum(frame[0]) && mu_isnum(frame[1])) {
        frame[0] = num_div(frame[0], frame[1]);
    } else if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_div(frame[0], frame[1]);
    } else {
        assert(!!!"incompatible types");
    }
        
    return 1;
}

static mc_t b_idiv(mu_t *frame) {
    if (mu_isnum(frame[0]) && mu_isnum(frame[1])) {
        frame[0] = num_idiv(frame[0], frame[1]);
    } else if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_div(frame[0], frame[1]);
    } else {
        assert(!!!"incompatible types");
    }
        
    return 1;
}

static mc_t b_mod(mu_t *frame) {
    if (mu_isnum(frame[0]) && mu_isnum(frame[1])) {
        frame[0] = num_mod(frame[0], frame[1]);
    } else if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_mod(frame[0], frame[1]);
    } else {
        assert(!!!"incompatible types");
    }
        
    return 1;
}

static mc_t b_sub(mu_t *frame) {
    if (mu_isnum(frame[0]) && !frame[1]) {
        frame[0] = num_neg(frame[0]);
        return 1;
    } else if (mu_isstr(frame[0]) && !frame[1]) {
        frame[0] = str_neg(frame[0]);
        return 1;
    } else if (mu_isnum(frame[0]) && mu_isnum(frame[1])) {
        frame[0] = num_sub(frame[0], frame[1]);
        return 1;
    } else if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_sub(frame[0], frame[1]);
        return 1;
    } else {
        assert(!!!"incompatible types");
    }
}

static mc_t b_xor(mu_t *frame) {
    if (mu_isstr(frame[0]) && !frame[1]) {
        frame[0] = str_not(frame[0]);
        return 1;
    } else if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_xor(frame[0], frame[1]);
        return 1;
    } else {
        assert(!!!"incompatible types");
    }
}

static mc_t b_and(mu_t *frame) {
    if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_and(frame[0], frame[1]);
        return 1;
    } else {
        assert(!!!"incompatible types");
    }
}

static mc_t b_or(mu_t *frame) {
    if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_or(frame[0], frame[1]);
        return 1;
    } else {
        assert(!!!"incompatible types");
    }
}

static mc_t b_concat(mu_t *frame) {
    if (mu_isstr(frame[0]) && mu_isstr(frame[1])) {
        frame[0] = str_concat(frame[0], frame[1]);
        mu_dec(frame[2]);
    } else if (mu_istbl(frame[0]) && mu_istbl(frame[1])) {
        frame[0] = tbl_concat(frame[0], frame[1], frame[2]);
    } else {
        assert(!!!"incompatible types");
    }
        
    return 1;
}

static mc_t b_pop(mu_t *frame) {
    assert(mu_istbl(frame[0]));
//    if (!frame[1]) {
//        frame[1] = muint(tbl_len(frame[0])-1);
//    }
    frame[0] = tbl_pop(frame[0], frame[1]);
    return 1;
}

static mc_t b_push(mu_t *frame) {
    assert(mu_istbl(frame[0]));
    if (!frame[2]) {
        frame[2] = muint(tbl_len(frame[0]));
    }
    tbl_push(frame[0], frame[1], frame[2]);
    return 0;
}

static mc_t b_bind(mu_t *frame) {
    mu_t f = tbl_pop(frame[0], muint(0));
    assert(mu_isfn(f));
    frame[0] = fn_bind(f, frame[0]);
    return 1;
}

static mc_t b_map(mu_t *frame) {
    assert(mu_isfn(frame[0]));
    frame[0] = fn_map(frame[0], frame[1]);
    return 1;
}

static mc_t b_filter(mu_t *frame) {
    assert(mu_isfn(frame[0]));
    frame[0] = fn_filter(frame[0], frame[1]);
    return 1;
}

static mc_t b_reduce(mu_t *frame) {
    mu_t f = tbl_pop(frame[0], muint(0));
    mu_t i = tbl_pop(frame[0], muint(0));
    assert(mu_isfn(f));
    frame[0] = fn_reduce(f, i, frame[0]);
    return 0xf;
}

static mc_t b_equals(mu_t *frame) {
    bool r = frame[0] == frame[1];
    mu_dec(frame[0]); mu_dec(frame[1]);
    frame[0] = r ? muint(1) : mnil;
    return 1;
}

static mc_t b_repr(mu_t *frame) {
    mu_t r = mu_repr(frame[0]);
    frame[0] = r;
    return 1;
}

static mc_t b_print(mu_t *frame) {
    mu_t k, v;
    for (muint_t i = 0; tbl_next(frame[0], &i, &k, &v);) {
        printvar(v);
    }

    mu_dec(frame[0]);
    printf("\n");
    return 0;
}

static mc_t b_test(mu_t *frame) {
    frame[0] = muint(0);
    frame[1] = muint(1);
    frame[2] = muint(2);
    frame[3] = muint(3);

    return 4;
}

static mc_t b_iter(mu_t *frame) {
    frame[0] = mu_iter(frame[0]);
    return 1;
}

static void genscope() {
    mu_t tbltbl = mctbl({
        { mcstr("concat"), mcfn(0x3, b_concat) },
        { mcstr("pop"), mcfn(0x2, b_pop) },
        { mcstr("push"), mcfn(0x3, b_push) },
    });

    mu_t fntbl = mctbl({
        { mcstr("bind"), mcfn(0xf, b_bind) },
        { mcstr("map"), mcfn(0x2, b_map) },
        { mcstr("filter"), mcfn(0x2, b_filter) },
        { mcstr("reduce"), mcfn(0xf, b_reduce) },
    });

    scope = tbl_extend(0, mctbl({
        { mcstr("+"), mcfn(0x2, b_add) },
        { mcstr("*"), mcfn(0x2, b_mul) },
        { mcstr("/"), mcfn(0x2, b_div) },
        { mcstr("//"), mcfn(0x2, b_idiv) },
        { mcstr("%"), mcfn(0x2, b_mod) },
        { mcstr("-"), mcfn(0x2, b_sub) },
        { mcstr("~"), mcfn(0x2, b_xor) },
        { mcstr("|"), mcfn(0x2, b_or) },
        { mcstr("&"), mcfn(0x2, b_and) },
        { mcstr("=="), mcfn(0x2, b_equals) },
        { mcstr("++"), mcfn(0x3, b_concat) },
        { mcstr("repr"), mcfn(0x1, b_repr) },
        { mcstr("print"), mcfn(0xf, b_print) },
        { mcstr("test"), mcfn(0x0, b_test) },
        { mcstr("tbl"), tbltbl },
        { mcstr("fn_"), fntbl },
        { mcstr("concat"), tbl_lookup(tbltbl, mcstr("concat")) },
        { mcstr("pop"), tbl_lookup(tbltbl, mcstr("pop")) },
        { mcstr("push"), tbl_lookup(tbltbl, mcstr("push")) },
        { mcstr("bind"), tbl_lookup(fntbl, mcstr("bind")) },
        { mcstr("map"), tbl_lookup(fntbl, mcstr("map")) },
        { mcstr("filter"), tbl_lookup(fntbl, mcstr("filter")) },
        { mcstr("reduce"), tbl_lookup(fntbl, mcstr("reduce")) },
        { mcstr("iter"), mcfn(0x1, b_iter) }
    }));
}

static int genargs(int i, int argc, const char **argv) {
    mu_t args = tbl_create(argc-i);

    for (muint_t j = 0; j < argc-i; j++) {
        tbl_insert(args, muint(j), mzstr(argv[i]));
    }

    // TODO use this?
    mu_dec(args);
    return i;
}


static void execute(const char *input) {
    struct code *c = parse_fn(mzstr(input));
    mu_t frame[MU_FRAME];
    mc_t rets = mu_exec(c, tbl_create(c->scope), frame);
    mu_fto(0, rets, frame);
}

static void load_file(FILE *file) {
    mbyte_t *buffer = mu_alloc(BUFFER_SIZE);
    size_t off = 0;

    size_t len = fread(buffer, 1, BUFFER_SIZE, file);

    if (ferror(file)) {
        mu_cerr(mcstr("io"),
                mcstr("encountered file reading error"));
    }

    struct code *c = parse_fn(mnstr(buffer+off, len-off));
    mu_dealloc(buffer, BUFFER_SIZE);

    mu_t s = tbl_extend(c->scope, mu_inc(scope));
    mu_t frame[MU_FRAME];
    mc_t rets = mu_exec(c, s, frame);
    mu_fto(0, rets, frame);
}

static void load(const char *name) {
    FILE *file;

    if (!(file = fopen(name, "r"))) {
        mu_cerr(mcstr("io"),
                mcstr("could not open file"));
    }

    load_file(file);

    fclose(file);
}

static mu_noreturn interpret() {
    mbyte_t *buffer = mu_alloc(BUFFER_SIZE);

    while (1) {
        mlen_t len = prompt(buffer);
        mu_t code = mnstr(buffer, len);

        mu_try_begin {
            mu_t frame[MU_FRAME];
            struct code *c = parse_fn(code);
            mc_t rets = mu_exec(c, mu_inc(scope), frame);
            mu_fto(0xf, rets, frame);
//
//            mu_try_begin {
//                f = fn_create_expr(0, code);
//            } mu_on_err (err) {
//                f = fn_create(0, code);
//            } mu_try_end;

//            mu_t output = fn_call_in(f, 0, scope);

            // TODO use strs?
//            mu_dis(f->code);

            printoutput(frame[0]);
            mu_dec(frame[0]);
        } mu_on_err (err) {
            printerr(err);
        } mu_try_end;
    }
}

static int run() {
    mu_t mainfn = tbl_lookup(scope, mcstr("main"));

    if (!mainfn)
        return 0;

    mu_t code = mu_call(mainfn, 0xf1, args);

    if (!code || mu_isnum(code))
        return num_int(code);
    else
        return -1;
}


static void usage(const char *name) {
    printf("\n"
           "usage: %s [options] [program] [args]\n"
           "options:\n"
           "  -e string     execute string before program\n"
           "  -l file       import and execute file before program\n"
           "  -i            run interactively after program\n"
           "  --            stop handling options\n"
           "program: file to execute and run or '-' for stdin\n"
           "args: arguments passed to running program\n"
           "\n", name);
}

static int options(int i, int argc, const char **argv) {
    while (i < argc) {
        muint_t len = strlen(argv[i]);

        if (argv[i][0] != '-') {
            return i;
        } else if (len > 2) {
            return -1;
        }

        switch (argv[i++][1]) {
            case 'e':
                if (i >= argc)
                    return -1;
                execute(argv[i++]);
                do_default = false;
                break;

            case 'l':
                if (i >= argc)
                    return -1;
                load(argv[i++]);
                break;

            case 'i':
                do_interpret = true;
                break;

            case '\0':
                do_stdin = true;
                return i;

            case '-':
                return i;

            default:
                return -1;
        }
    }

    return i;
}


int main(int argc, const char **argv) {
    mu_try_begin {
        int i = 1;

        genscope();

        if ((i = options(i, argc, argv)) < 0) {
            usage(argv[0]);
            return -2;
        }

        if (i >= argc && !do_stdin) {
            if (!do_default && !do_interpret)
                return 0;
            else
                do_interpret = true;
        }

        if (i < argc || do_stdin) {
            if (do_stdin)
                load_file(stdin);
            else
                load(argv[i++]);
        } else if (do_default) {
            do_interpret = true;
        } else if (!do_interpret) {
            return 0;
        }

        genargs(i, argc, argv);

        if (do_interpret)
            interpret();
        else
            return run();

    } mu_on_err (err) {
        printerr(err);

        // TODO fix this with constant allocations
        mu_t code = tbl_lookup(err, mcstr("code"));
        if (mu_isnum(code))
            return num_int(code);
        else
            return -1;
    } mu_try_end;
}

