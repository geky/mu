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

#include <string.h>
#include <stdio.h>
#include <assert.h>

#define PROMPT_A "\033[32m> \033[0m"
#define PROMPT_B "\033[32m. \033[0m"
#define OUTPUT_A ""

#define ERR_START "\033[31m"
#define ERR_END   "\033[0m"

#define BUFFER_SIZE MU_MAXLEN


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
    tbl_for_begin (k, v, f) {
        if (!mu_isnum(k)) {
            printrepr(k);
            printf(": ");
        }

        printrepr(v);

        if (--i != 0) {
            printf(", ");
        }
    } tbl_for_end
    printf("\n");
}

static len_t prompt(byte_t *input) {
    printf("%s", PROMPT_A);
    len_t len = 0;

    while (1) {
        byte_t c = getchar();
        input[len++] = c;
        
        if (c == '\n')
            return len;
    }
}


// TODO move this scope declaration somewhere else
static frame_t b_add(mu_t *frame) {
    assert(mu_isnum(frame[0]) && mu_isnum(frame[1]));
    frame[0] = mdouble(num_double(frame[0]) + num_double(frame[1]));
    return 1;
}

static frame_t b_mul(mu_t *frame) {
    assert(mu_isnum(frame[0]) && mu_isnum(frame[1]));
    frame[0] = mdouble(num_double(frame[0]) * num_double(frame[1]));
    return 1;
}

static frame_t b_sub(mu_t *frame) {
    if (!frame[1]) {
        frame[0] = mdouble(-num_double(frame[0]));
        return 1;
    } else {
        assert(mu_isnum(frame[0]) && mu_isnum(frame[1]));
        frame[0] = mdouble(num_double(frame[0]) - num_double(frame[1]));
        return 1;
    }
}

static frame_t b_concat(mu_t *frame) {
    assert(mu_istbl(frame[0]) && mu_istbl(frame[1]));
    frame[0] = tbl_concat(frame[0], frame[1], frame[2]);
    return 1;
}

static frame_t b_pop(mu_t *frame) {
    assert(mu_istbl(frame[0]));
    if (!frame[1]) {
        frame[1] = muint(tbl_len(frame[0])-1);
    }
    frame[0] = tbl_pop(frame[0], frame[1]);
    return 1;
}

static frame_t b_push(mu_t *frame) {
    assert(mu_istbl(frame[0]));
    if (!frame[2]) {
        frame[2] = muint(tbl_len(frame[0]));
    }
    tbl_push(frame[0], frame[1], frame[2]);
    return 0;
}

static frame_t b_equals(mu_t *frame) {
    bool r = mu_equals(frame[0], frame[1]);
    mu_dec(frame[0]); mu_dec(frame[1]);
    frame[0] = r ? muint(1) : mnil;
    return 1;
}

static frame_t b_repr(mu_t *frame) {
    mu_t r = mu_repr(frame[0]);
    mu_dec(frame[0]);
    frame[0] = r;
    return 1;
}

static frame_t b_print(mu_t *frame) {
    tbl_for_begin (k, v, frame[0]) {
        printvar(v);
    } tbl_for_end;

    mu_dec(frame[0]);
    printf("\n");
    return 0;
}

static frame_t b_test(mu_t *frame) {
    frame[0] = muint(0);
    frame[1] = muint(1);
    frame[2] = muint(2);
    frame[3] = muint(3);

    return 4;
}

static void genscope() {
    scope = tbl_create(0);

    mu_t ops = tbl_create(0);
    tbl_assign(ops, mcstr("+"), mbfn(0x2, b_add));
    tbl_assign(ops, mcstr("*"), mbfn(0x2, b_mul));
    tbl_assign(ops, mcstr("-"), mbfn(0x2, b_sub));
    tbl_assign(ops, mcstr("=="), mbfn(0x2, b_equals));
    tbl_assign(scope, mcstr("+"), mbfn(0x2, b_add));
    tbl_assign(scope, mcstr("-"), mbfn(0x2, b_sub));
    tbl_assign(scope, mcstr("*"), mbfn(0x2, b_mul));
    tbl_assign(scope, mcstr("=="), mbfn(0x2, b_equals));
    tbl_assign(scope, mcstr("ops"), ops);
    tbl_assign(scope, mcstr("repr"), mbfn(0x1, b_repr));
    tbl_assign(scope, mcstr("print"), mbfn(0xf, b_print));
    tbl_assign(scope, mcstr("test"), mbfn(0x0, b_test));
    mu_t tbltbl = tbl_create(0);
    tbl_assign(tbltbl, mcstr("concat"), mbfn(0x3, b_concat));
    tbl_assign(tbltbl, mcstr("pop"), mbfn(0x2, b_pop));
    tbl_assign(tbltbl, mcstr("push"), mbfn(0x3, b_push));
    tbl_assign(scope, mcstr("tbl"), tbltbl);
    tbl_assign(scope, mcstr("concat"), tbl_lookup(tbltbl, mcstr("concat")));
    tbl_assign(scope, mcstr("pop"), tbl_lookup(tbltbl, mcstr("pop")));
    tbl_assign(scope, mcstr("push"), tbl_lookup(tbltbl, mcstr("push")));
    
}

static int genargs(int i, int argc, const char **argv) {
    mu_t args = tbl_create(argc-i);

    for (uint_t j = 0; j < argc-i; j++) {
        tbl_insert(args, muint(j), mcstr(argv[i]));
    }

    return i;
}


static void execute(const char *input) {
    mu_t f = fn_parse_fn(mcstr(input), 0);

    //fn_call_in(f, 0, scope);
    fn_call(f, 0x00);
}

static void load_file(FILE *file) {
    byte_t *buffer = mu_alloc(BUFFER_SIZE);
    size_t off = 0;

    size_t len = fread(buffer, 1, BUFFER_SIZE, file);

    if (ferror(file)) {
        mu_cerr(mcstr("io"), 
                mcstr("encountered file reading error"));
    }

    if (!memcmp(buffer, "#!", 2)) {
        for (off = 2; buffer[off] != '\n'; off++)
            ;

        off++;
    }

    mu_t f = fn_parse_fn(mnstr(buffer+off, len-off), scope);

    //fn_call_in(f, 0, scope);
    fn_call(f, 0x00);
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
    byte_t *buffer = mu_alloc(BUFFER_SIZE);

    while (1) {
        len_t len = prompt(buffer);
        mu_t code = mnstr(buffer, len);
        
        mu_try_begin {
            mu_t f = fn_parse_fn(code, scope);
//    
//            mu_try_begin {
//                f = fn_create_expr(0, code);
//            } mu_on_err (err) {
//                f = fn_create(0, code);
//            } mu_try_end;

//            mu_t output = fn_call_in(f, 0, scope);

            // TODO use strs?
//            mu_dis(f->code);

            mu_t output = fn_call(f, 0x0f);
            printoutput(output);
            mu_dec(output);
            fn_dec(f);
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
        uint_t len = strlen(argv[i]);

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

