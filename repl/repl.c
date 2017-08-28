/*
 * Mu repl library for presenting a user-interactive interface
 *
 * Copyright (c) 2016 Christopher Haster
 * Distributed under the MIT license in mu.h
 */
#include "repl.h"


#if MU_USE_STD_TERM
// tie into unistd stuff
#include <stdio.h>
#include <termios.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

static bool registered = false;
static struct termios termorig;

int mu_sys_termenter(void) {
    if (tcgetattr(0, &termorig)) {
        mu_errorf("termios error: %d", errno);
    }

    if (!registered) {
        atexit(mu_sys_termexit);
        registered = true;
    }

    static struct termios termnew;
    termnew = termorig;
    termnew.c_lflag &= ~ICANON & ~ECHO;

    if (tcsetattr(0, TCSANOW, &termnew)) {
        mu_errorf("termios error: %d", errno);
    }

    return 0;
}

void mu_sys_termexit(void) {
    if (tcsetattr(0, TCSANOW, &termorig)) {
        mu_errorf("termios error: %d", errno);
    }
}

mint_t mu_sys_termread(void *buf, muint_t n) {
    return read(STDIN_FILENO, buf, n);
}

mint_t mu_sys_termwrite(const void *buf, muint_t n) {
    return write(STDOUT_FILENO, buf, n);
}

#endif


// sys wrappers for repl
static char mu_repl_getc(void) {
    char b;
    if (mu_sys_termread(&b, 1) < 0) {
        mu_errorf("read error: %d", errno);
    }

    return b;
}

static void mu_repl_write(const void *buf, size_t n) {
    if (mu_sys_termwrite(buf, n) < 0) {
        mu_errorf("write error: %d", errno);
    }
}

struct mrepl {
    mu_t line;
    muint_t n;

    int pos;
    int off;
    int cols;
    bool compat;

    int hpos;
    mu_t disp;
    const char *prompt;
};

static mu_t mu_repl_history = MU_NIL;

static void mu_repl_edit(struct mrepl *r) {
    if (r->hpos == -1) {
        return;
    }

    r->n = 0;
    mu_buf_pushmu(&r->line, &r->n, 
            mu_tbl_lookup(mu_repl_history,
                mu_num_fromuint(r->hpos)));
    r->hpos = -1;
}

static void mu_repl_update(struct mrepl *r) {
    mu_t line;
    int linesize;
    if (r->hpos == -1) {
        line = mu_inc(r->line);
        linesize = r->n;
    } else {
        line = mu_tbl_lookup(mu_repl_history,
                mu_num_fromuint(r->hpos));
        linesize = mu_buf_getlen(line);
    }

    if (r->pos > linesize) {
        r->pos = linesize;
    } else if (r->pos < 0) {
        r->pos = 0;
    }

    int plen = strlen(r->prompt);
    if (r->pos - r->off > r->cols - plen - 1) {
        r->off = r->pos - (r->cols - plen - 1);
    } else if (r->pos - r->off < 0) {
        r->off = r->pos;
    }

    linesize -= r->off;
    if (linesize > r->cols - plen) {
        linesize = r->cols - plen;
    }

    muint_t n = 0;
    mu_buf_pushf(&r->disp, &n,
            "\r\x1b[K%s%s%s%n\r\x1b[%dC",
            r->compat ? "\x1b[32m" : "",
            r->prompt,
            r->compat ? "\x1b[m" : "",
            (mbyte_t *)mu_buf_getdata(line) + r->off,
            linesize,
            r->pos - r->off + plen);
    mu_repl_write(mu_buf_getdata(r->disp), n);
    mu_dec(line);
}

static bool mu_repl_istabable(char c) {
    return (
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '0') ||
        c == '.' || c == '_');
}

static void mu_repl_tab(struct mrepl *r, mu_t scope, bool show) {
    const char *data = mu_buf_getdata(r->line);
    muint_t prefix = 0;
    while (prefix < r->pos && mu_repl_istabable(data[r->pos-1-prefix])) {
        prefix++;
    }

    while (true) {
        muint_t j = 0;
        while (j < prefix && data[r->pos-prefix+j] != '.') {
            j++;
        }

        if (j == prefix) {
            break;
        }

        mu_t word = mu_str_fromdata(&data[r->pos-prefix], j);
        mu_t next = mu_tbl_lookup(scope, word);
        mu_dec(scope);
        if (!next || !mu_istbl(next)) {
            mu_dec(next);
            return;
        }

        scope = next;
        prefix -= j+1;
    }

    mu_t best = MU_NIL;
    mu_t results;
    if (show) {
        results = mu_tbl_create(0);
    }

    while (scope) {
        mu_t k;
        for (muint_t i = 0; mu_tbl_next(scope, &i, &k, NULL);) {
            if (!mu_isstr(k) || prefix > mu_str_getlen(k) ||
                memcmp(&data[r->pos-prefix], mu_str_getdata(k), prefix) != 0) {
                mu_dec(k);
                continue;
            }

            if (show) {
                mu_tbl_push(results, mu_inc(k), mu_tbl_getlen(results));
            }

            if (!best) {
                best = k;
            } else {
                muint_t i = 0;
                for (; i < mu_str_getlen(best); i++) {
                    if (((mbyte_t*)mu_str_getdata(best))[i] != 
                        ((mbyte_t*)mu_str_getdata(k))[i]) {
                        break;
                    }
                }

                best = mu_fn_call(MU_SUBSET, 0x31, best,
                        mu_num_fromuint(0), mu_num_fromuint(i));
                mu_dec(k);
            }
        }

        mu_t tail = mu_tbl_gettail(scope);
        mu_dec(scope);
        scope = tail;
    }

    if (show) {
        mu_repl_write("\n", 1);
        int maxwidth = 0;
        mu_t v;
        for (muint_t i = 0; mu_tbl_next(results, &i, NULL, &v);) {
            if (mu_str_getlen(v) > maxwidth) {
                maxwidth = mu_str_getlen(v);
            }
            mu_dec(v);
        }

        int cols = r->cols / (maxwidth+2);
        int rows = (mu_tbl_getlen(results)+cols-1) / cols;
        for (muint_t i = 0; i < rows; i++) {
            for (muint_t j = 0; j < cols &&
                        j*rows + i < mu_tbl_getlen(results); j++) {
                mu_t s = mu_fn_call(MU_PAD, 0x21,
                        mu_tbl_lookup(results, mu_num_fromuint(j*rows + i)),
                        mu_num_fromuint(maxwidth+2));
                mu_repl_write(mu_str_getdata(s), mu_str_getlen(s));
                mu_dec(s);
            }

            mu_repl_write("\n", 1);
        }

        mu_dec(results);
    }

    if (best) {
        int diff = mu_str_getlen(best) - prefix;
        mu_buf_push(&r->line, &r->n, diff);
        memmove((mbyte_t*)mu_buf_getdata(r->line) + r->pos + diff,
                (mbyte_t*)mu_buf_getdata(r->line) + r->pos,
                r->n-diff - r->pos);
        memcpy((mbyte_t*)mu_buf_getdata(r->line) + r->pos,
                (const mbyte_t*)mu_str_getdata(best) + prefix, diff);
        mu_dec(best);
        r->pos += diff;
    }
}

mu_t mu_repl_read(const char *prompt, mu_t scope) {
    if (!mu_repl_history) {
        mu_repl_history = mu_tbl_create(0);
    }

    struct mrepl r = {
        .line = mu_buf_create(0),
        .n = 0,
        .pos = 0,
        .off = 0,
        .cols = 4096,
        .compat = false,
        .hpos = -1,
        .disp = mu_buf_create(0),
        .prompt = prompt,
    };

    int err = mu_sys_termenter();
    if (err) {
        mu_errorf("termenter failed: %d", err);
    }

    // just move cursor to end to find width, also checks if escape codes
    // work. parsing is left up to the main loop.
    mu_repl_write("\x1b[4096C\x1b[6n\r", 12);

    char c;
    char prev;

    while (1) {
        mu_repl_update(&r);

        prev = c;
        c = mu_repl_getc();

        if (c == '\x0a') { // enter
            mu_repl_edit(&r);
            break;
        } else if (c == '\t') { // tab
            mu_repl_tab(&r, mu_inc(scope), prev == '\t');
        } else if (c == '\x7f') { // backspace
            mu_repl_edit(&r);
            if (r.pos > 0) {
                memmove((mbyte_t*)mu_buf_getdata(r.line) + r.pos - 1,
                        (mbyte_t*)mu_buf_getdata(r.line) + r.pos,
                        r.n - r.pos);
                r.n -= 1;
                r.pos -= 1;
            }
        } else if (c == '\x1b') { // escape code
            c = mu_repl_getc();
            if (c != '[') continue;

            c = mu_repl_getc();
            if (c == 'D') { // left
                r.pos -= 1;
            } else if (c == 'C') { // right
                r.pos += 1;
            } else if (c == 'A') { // up
                mu_t h = mu_tbl_lookup(mu_repl_history,
                        mu_num_fromuint(r.hpos + 1));
                if (h) {
                    r.hpos += 1;
                    r.pos = mu_buf_getlen(h);
                    mu_dec(h);
                }
            } else if (c == 'B') { // down
                if (r.hpos == 0) {
                    r.hpos = -1;
                    r.pos = r.n;
                } else {
                    mu_t h = mu_tbl_lookup(mu_repl_history,
                            mu_num_fromuint(r.hpos - 1));
                    if (h) {
                        r.hpos -= 1;
                        r.pos = mu_buf_getlen(h);
                        mu_dec(h);
                    }
                }
            } else if (c >= '0' && c <= '9') {
                // special codes
                int code;
                while (true) {
                    code = 0;
                    while (c >= '0' && c <= '9') {
                        code = code*10 + (c - '0');
                        c = mu_repl_getc();
                    }

                    if (c != ';') {
                        break;
                    }

                    c = mu_repl_getc();
                }

                if (c == 'R') { // terminal size
                    r.cols = code;
                    r.compat = true;
                } else if (code == 1) { // home
                    r.pos = 0;
                } else if (code == 4) { // end
                    r.pos = 4096;
                } else if (code == 3) { // delete 
                    mu_repl_edit(&r);
                    if (r.pos < r.n) {
                        memmove((mbyte_t*)mu_buf_getdata(r.line) + r.pos,
                                (mbyte_t*)mu_buf_getdata(r.line) + r.pos + 1,
                                r.n - r.pos);
                        r.n -= 1;
                    }
                }
            }
        } else {
            mu_repl_edit(&r);
            mu_buf_push(&r.line, &r.n, 1);
            memmove((mbyte_t*)mu_buf_getdata(r.line) + r.pos + 1,
                    (mbyte_t*)mu_buf_getdata(r.line) + r.pos,
                    r.n-1 - r.pos);
            ((mbyte_t*)mu_buf_getdata(r.line))[r.pos] = c;
            r.pos += 1;
        }
    }

    mu_repl_write("\n", 1);
    mu_sys_termexit();
    mu_dec(r.disp);
    mu_buf_resize(&r.line, r.n);

    mu_tbl_insert(mu_repl_history, mu_num_fromuint(MU_REPL_HISTORY), 0);
    mu_tbl_push(mu_repl_history, mu_inc(r.line), 0);

    return r.line;
}

void mu_repl_feval(const char *prompt,
        mu_t scope, mcnt_t fc, mu_t *frame) {
    mu_t line = mu_repl_read(prompt, scope);
    mu_t c = mu_compile(mu_buf_getdata(line),
            mu_buf_getlen(line), mu_inc(scope));
    mcnt_t rets = mu_exec(c, mu_inc(scope), frame);
    mu_frameconvert(rets, fc, frame);
}

mu_t mu_repl_veval(const char *prompt,
        mu_t scope, mcnt_t fc, va_list args) {
    mu_t frame[MU_FRAME];

    mu_repl_feval(prompt, scope, fc, frame);

    for (muint_t i = 1; i < mu_framecount(fc); i++) {
        *va_arg(args, mu_t *) = frame[i];
    }

    return fc ? *frame : 0;
}

mu_t mu_repl_eval(const char *prompt,
        mu_t scope, mcnt_t fc, ...) {
    va_list args;
    va_start(args, fc);
    mu_t ret = mu_repl_veval(prompt, scope, fc, args);
    va_end(args);
    return ret;
}

void mu_repl(const char *prompt, mu_t scope) {
    mu_t frame[MU_FRAME];

    mu_repl_feval(prompt, scope, 0xf, frame);

    if (mu_tbl_getlen(frame[0]) > 0) {
        frame[1] = mu_num_fromuint(2);
        mu_fn_fcall(MU_REPR, 0x21, frame);

        mu_print((const char *)mu_str_getdata(frame[0]) + 1,
                mu_str_getlen(frame[0])-2);
    }

    mu_dec(frame[0]);
}
