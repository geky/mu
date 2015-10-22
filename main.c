/*
 * Mu stand-alone interpreter
 */

#include "mu.h"
#include "num.h"
#include "str.h"
#include "tbl.h"
#include "fn.h"
#include "parse.h"
#include "vm.h"
#include "sys.h"

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <setjmp.h>

#define PROMPT_A "\033[32m> \033[0m"
#define PROMPT_B "\033[32m. \033[0m"
#define OUTPUT_A ""

#define ERR_START "\033[31m"
#define ERR_END   "\033[0m"

#define BUFFER_SIZE 1<<15


static mu_t scope = 0;
static mu_t args = 0;

static bool do_interpret = false;
static bool do_stdin = false;
static bool do_default = true;


// System functions
const char *error_message;
muint_t error_len;
jmp_buf error_handler;

struct error_handler {
    const char *message;
    muint_t len;
    jmp_buf buf;
} eh;

mu_noreturn sys_error(const char *m, muint_t len) {
    eh.message = m;
    eh.len = len;

    longjmp(eh.buf, 1);
}

void sys_print(const char *m, muint_t len) {
    fwrite(m, sizeof(mbyte_t), len, stdout);
    fputc('\n', stdout);
}

mu_t sys_import(mu_t name) {
    mu_dec(name);
    return 0;
}


static void printvar(mu_t v) {
    if (!mu_isstr(v)) {
        v = mu_repr(v);
    }

    printf("%.*s", str_len(v), str_bytes(v));
}

static void printrepr(mu_t v) {
    printvar(mu_dump(v, MU_INF, 0));
}

static void printerr(void) {
    printf("%s", ERR_START);
    printf("error: %.*s", eh.len, eh.message);
    printf("%s\n", ERR_END);
}

static void printoutput(mu_t f) {
    int i = tbl_len(f);

    if (i == 0)
        return;

    printf("%s", OUTPUT_A);

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

static void genscope() {
    scope = mu_tbl(MU_BUILTINS, 0);
}

static int genargs(int i, int argc, const char **argv) {
    mu_t args = tbl_create(argc-i);

    for (muint_t j = 0; j < argc-i; j++) {
        tbl_insert(args, muint(j), mcstr(argv[i]));
    }

    // TODO use this?
    mu_dec(args);
    return i;
}


static void execute(const char *input) {
    struct code *c = mu_compile(mcstr(input));
    mu_t frame[MU_FRAME];
    mc_t rets = mu_exec(c, tbl_create(c->scope), frame);
    mu_fconvert(0, rets, frame);
}

static void load_file(FILE *file) {
    mbyte_t *buffer = mu_alloc(BUFFER_SIZE);
    size_t off = 0;

    size_t len = fread(buffer, 1, BUFFER_SIZE, file);

    if (ferror(file)) {
        mu_error(mcstr("io error reading file"));
    }

    struct code *c = mu_compile(mnstr(buffer+off, len-off));
    mu_dealloc(buffer, BUFFER_SIZE);

    mu_t s = tbl_extend(c->scope, mu_inc(scope));
    mu_t frame[MU_FRAME];
    mc_t rets = mu_exec(c, s, frame);
    mu_fconvert(0, rets, frame);
}

static void load(const char *name) {
    FILE *file;

    if (!(file = fopen(name, "r"))) {
        mu_error(mcstr("io error opening file"));
    }

    load_file(file);

    fclose(file);
}

static mu_noreturn interpret() {
    mbyte_t *buffer = mu_alloc(BUFFER_SIZE);

    while (1) {
        mlen_t len = prompt(buffer);
        mu_t code = mnstr(buffer, len);

        struct error_handler old_eh = eh;
        if (!setjmp(eh.buf)) {
            mu_t frame[MU_FRAME];
            struct code *c = mu_compile(code);
            mc_t rets = mu_exec(c, mu_inc(scope), frame);
            mu_fconvert(0xf, rets, frame);

            printoutput(frame[0]);
            mu_dec(frame[0]);
        } else {
            printerr();
        }
        eh = old_eh;
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
    if (!setjmp(eh.buf)) {
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

    } else {
        printerr();
        return -1;
    }
}

