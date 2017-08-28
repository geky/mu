/*
 * Mu example program for unix-like systems
 *
 * Copyright (c) 2016 Christopher Haster
 * Distributed under the MIT license in mu.h
 */
#include "mu/mu.h"
#include "repl/repl.h"

#ifndef MU_NO_DIS
#include "dis/dis.h"
#define MU_DIS_ENTRY { mu_dis_key_def, mu_dis_module_def }
#else
#define MU_DIS_ENTRY { NULL, NULL }
#endif

#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <errno.h>

#define BLOCK_SIZE 512
#define HISTORY_LEN 256


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
    scope = mu_tbl_createtail(0, MU_BUILTINS);
}

static void init_args(void) {
    args = mu_tbl_create(0);
    while (*argv) {
        mu_tbl_insert(args, mu_num_fromuint(mu_tbl_getlen(args)),
                mu_str_format("%s", *argv++));
    }
}


// System functions
jmp_buf error_jmp;

mu_noreturn mu_sys_error(const char *m, muint_t len) {
    printf("\x1b[31merror: %.*s\x1b[0m\n", (unsigned)len, m);
    longjmp(error_jmp, 1);
}

void mu_sys_print(const char *m, muint_t len) {
    printf("%.*s\n", (unsigned)len, m);
}


MU_DEF_TBL(mu_sys_imports_def, {
    MU_DIS_ENTRY,
})

mu_t mu_sys_import(mu_t name) {
    mu_t module = mu_tbl_lookup(mu_sys_imports_def(), name);
    return module;
}


// Operations
static void execute(const char *input) {
    if (!setjmp(error_jmp)) {
        mu_eval(input, strlen(input), scope, 0);
    }
}

static void load_file(FILE *file) {
    if (!setjmp(error_jmp)) {
        mu_t buffer = mu_buf_create(0);
        muint_t n = 0;

        while (true) {
            mu_buf_push(&buffer, &n, BLOCK_SIZE);
            size_t read = fread(
                    (mbyte_t*)mu_buf_getdata(buffer) + n - BLOCK_SIZE,
                    1, BLOCK_SIZE, file);

            if (read < BLOCK_SIZE) {
                break;
            }
        }

        if (ferror(file)) {
            mu_errorf("io error reading file (%d)", errno);
        }

        mu_eval(mu_buf_getdata(buffer), n, scope, 0);
        mu_dec(buffer);
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
        if (!setjmp(error_jmp)) {
            mu_repl("> ", scope);
        }
    }

    mu_unreachable;
}

static int run() {
    mu_t mainfn = mu_tbl_lookup(scope, mu_str_format("main"));
    if (!mainfn || !mu_isfn(mainfn)) {
        return 0;
    }

    mu_t code = mu_fn_call(mainfn, 0xf1, args);
    return mu_num_getint(code);
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
