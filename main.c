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
#include <setjmp.h>
#include <stdlib.h>
#include <errno.h>

#include <readline/readline.h>
#include <readline/history.h>

#define BLOCK_SIZE 512


// Global state 
const char **argv;

static struct {
    bool execute;
    bool interpret;
    bool load;
} mode;


// Mu state
static mu_t scope = 0;
static mu_t args = 0;

static void init_scope(void) {
    scope = tbl_extend(0, MU_BUILTINS);
}

static void init_args(void) {
    args = tbl_create(0);
    while (*argv) {
        tbl_push(args, mstr("%s", *argv++), 0);
    }
}


// System functions
jmp_buf error_jmp;

mu_noreturn sys_error(const char *m, muint_t len) {
    printf("\e[31merror: %.*s\e[0m\n", (unsigned)len, m);
    longjmp(error_jmp, 1);
}

void sys_print(const char *m, muint_t len) {
    printf("%.*s\n", (unsigned)len, m);
}

mu_t sys_import(mu_t name) {
    mu_dec(name);
    return 0;
}


// Operations
static mu_t execute_result(const char *input) {
    mu_t frame[MU_FRAME];
    mu_t s = mstr("%s", input);
    struct code *c = mu_compile(s);
    mc_t rets = mu_exec(c, mu_inc(scope), frame);
    mu_fconvert(0xf, rets, frame);
    return frame[0];
}

static void execute(const char *input) {
    if (!setjmp(error_jmp)) {
        mu_dec(execute_result(input));
    }
}

static void load_file(FILE *file) {
    if (!setjmp(error_jmp)) {
        mu_t buffer = buf_create(0);
        muint_t n = 0;

        while (true) {
            buf_expand(&buffer, buf_len(buffer)+BLOCK_SIZE);
            size_t read = fread((char *)buf_data(buffer) + n, 1, BLOCK_SIZE, file);
            n += read;

            if (read < BLOCK_SIZE) {
                break;
            }
        }

        if (ferror(file)) {
            mu_errorf("io error reading file (%d)", errno);
        }

        buf_push(&buffer, &n, '\0');
        execute(buf_data(buffer));
        buf_dec(buffer);
    }
}

static void load(const char *name) {
    if (!setjmp(error_jmp)) {
        FILE *file;
        if (!(file = fopen(name, "r"))) {
            mu_errorf("io error opening file (%d)", errno);
        }

        load_file(file);

        fclose(file);
    }
}

static int interpret() {
    while (true) {
        char *input = readline("\001\e[32m\002> \001\e[0m\002");
        if (!input) {
            return 0;
        }

        add_history(input);

        if (!setjmp(error_jmp)) {
            mu_t res = execute_result(input);
            mu_t repr = mu_dump(res, muint(2));
            printf("%.*s\n", str_len(repr)-2, str_bytes(repr)+1);
            mu_dec(repr);
        }

        free(input);
    }
}

static int run() {
    mu_t mainfn = tbl_lookup(scope, mstr("main"));
    if (!mainfn) {
        return 0;
    }

    mu_t code = mu_call(mainfn, 0xf1, args);
    return num_int(code);
}


// Entry point
static mu_noreturn usage(const char *name) {
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

    exit(-1);
}

static void options(void) {
    const char *name = *argv++;

    while (*argv && (*argv)[0] == '-') {
        muint_t len = strlen(*argv);
        if (len > 2) {
            usage(name);
        }

        switch ((*argv++)[1]) {
            case 'e':
                if (!*argv) {
                    usage(name);
                }

                execute(*argv++);
                mode.execute = true;
                break;

            case 'l':
                if (!*argv) {
                    usage(name);
                }

                load(*argv++);
                break;

            case 'i':
                mode.interpret = true;
                break;

            case '\0':
                mode.load = true;
                return;

            case '-':
                return;

            default:
                usage(name);
        }
    }
}

int main(int argc1, const char **argv1) {
    argv = argv1;

    init_scope();
    options();

    if (mode.load || *argv) {
        if (mode.load) {
            load_file(stdin);
        } else {
            load(*argv++);
        }

        mode.load = true;
    }

    init_args();

    if (mode.interpret || (!mode.load && !mode.execute)) {
        return interpret();
    } else {
        return run();
    }
}
