/*
 # This file is part of the Astrometry.net suite.
 # Licensed under a 3-clause BSD style license - see LICENSE
 */
#ifdef __GNUC__ //# Modified by Robert Lancaster for the StellarSolver Internal Library
#define __STDC_WANT_LIB_EXT2__ 1
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "os-features.h" //# Modified for the StellarSolver Internal Library for thread safety
#include "errors.h"
#include "ioutils.h"
#include "an-bool.h"
#include "log.h" //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging

//# Modified for the StellarSolver Internal Library for thread safety:
//  - Windows: FlsAlloc with destructor + InitOnceExecuteOnce (requires Vista+)
//  - POSIX: pthread_key_create with destructor + pthread_once
//  - atexit race fixed with pthread_once / InitOnceExecuteOnce
#if defined(_WIN32) || defined(_MSC_VER) || defined(__MINGW32__)
#include <windows.h>
static DWORD estack_fls_key = FLS_OUT_OF_INDEXES;

static void estack_destructor(void* val) {
    pl* stack = (pl*)val;
    if (stack) {
        int i;
        for (i = 0; i < pl_size(stack); i++) {
            err_t* e = pl_get(stack, i);
            error_free(e);
        }
        pl_free(stack);
    }
}

static VOID WINAPI estack_fls_destructor(PVOID val) {
    estack_destructor(val);
}

static INIT_ONCE tls_init_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK init_tls_callback(PINIT_ONCE once, PVOID param, PVOID *context) {
    (void)once; (void)param; (void)context;
    estack_fls_key = FlsAlloc(estack_fls_destructor);
    return TRUE;
}

static void init_tls() {
    InitOnceExecuteOnce(&tls_init_once, init_tls_callback, NULL, NULL);
}

static pl* get_estack() {
    init_tls();
    if (estack_fls_key == FLS_OUT_OF_INDEXES) return NULL;
    return (pl*)FlsGetValue(estack_fls_key);
}
static void set_estack(pl* val) {
    init_tls();
    if (estack_fls_key != FLS_OUT_OF_INDEXES) {
        FlsSetValue(estack_fls_key, val);
    }
}

static INIT_ONCE atexit_init_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK register_atexit_callback(PINIT_ONCE once, PVOID param, PVOID *context) {
    (void)once; (void)param; (void)context;
    atexit(errors_free);
    return TRUE;
}
static void register_atexit() {
    InitOnceExecuteOnce(&atexit_init_once, register_atexit_callback, NULL, NULL);
}

#else
#include <pthread.h>
static pthread_key_t estack_key;
static pthread_once_t estack_key_once = PTHREAD_ONCE_INIT;

static void estack_destructor(void* val) {
    pl* stack = (pl*)val;
    if (stack) {
        int i;
        for (i = 0; i < pl_size(stack); i++) {
            err_t* e = pl_get(stack, i);
            error_free(e);
        }
        pl_free(stack);
    }
}

static void make_estack_key() {
    pthread_key_create(&estack_key, estack_destructor);
}

static pl* get_estack() {
    pthread_once(&estack_key_once, make_estack_key);
    return (pl*)pthread_getspecific(estack_key);
}

static void set_estack(pl* val) {
    pthread_once(&estack_key_once, make_estack_key);
    pthread_setspecific(estack_key, val);
}

static pthread_once_t atexit_once = PTHREAD_ONCE_INIT;
static void register_atexit_fn() {
    atexit(errors_free);
}
static void register_atexit() {
    pthread_once(&atexit_once, register_atexit_fn);
}
#endif

static err_t* error_copy(err_t* e) {
    int i, N;
    err_t* copy = error_new();
    copy->print = e->print;
    copy->save = e->save;
    N = error_stack_N_entries(e);
    for (i=0; i<N; i++) {
        errentry_t* ee = error_stack_get_entry(e, i);
        error_stack_add_entry(copy, ee->file, ee->line, ee->func, ee->str);
    }
    return copy;
}

// NOT thread-safe: global FILE*. No callers of errors_print_on_exit() in StellarSolver.
static FILE* print_errs_fid;
static void print_errs(void) {
    FILE* fid = print_errs_fid;
    //debug(fid, "Error traceback:\n"); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
    errors_print_stack(fid);
}

void errors_start_logging_to_string() {
    err_t* err;
    errors_push_state();
    err = errors_get_state();
    err->print = NULL;
    err->save = TRUE;
}

char* errors_stop_logging_to_string(const char* separator) {
    err_t* err;
    char* rtn;
    err = errors_get_state();
    rtn = error_get_errs(err, separator);
    errors_pop_state();
    return rtn;
}

int errors_print_on_exit(FILE* fid) {
    err_t* e;
    errors_push_state();
    e = errors_get_state();
    e->save = TRUE;
    e->print = NULL;
    print_errs_fid = fid;
    return atexit(print_errs);
}

void errors_log_to(FILE* f) {
    err_t* e;
    e = errors_get_state();
    e->print = f;
}

void errors_use_function(errfunc_t* func, void* baton) {
    err_t* e;
    e = errors_get_state();
    e->errfunc = func;
    e->baton = baton;
    e->print = NULL;
    e->save = FALSE;
}

void errors_clear_stack() {
    error_stack_clear(errors_get_state());
}

err_t* errors_get_state() {
    pl* estack = get_estack();
    if (!estack) {
        estack = pl_new(4);
        set_estack(estack);
        register_atexit(); //# Modified for the StellarSolver Internal Library for thread safety
    } 
    if (!pl_size(estack)) {
        err_t* e = error_new();
        e->print = stderr;
        pl_append(estack, e);
        //free(e); //# Modified by Robert Lancaster for the StellarSolver Internal Library, preventing leaks, the append copies the object
        //This was in fact flagged as a leak, but deleting it may have caused crashes.
    }
    return pl_get(estack, pl_size(estack)-1);
}

void errors_free() {
    pl* estack = get_estack();
    int i;
    if (!estack)
        return;
    for (i=0; i<pl_size(estack); i++) {
        err_t* e = pl_get(estack, i);
        error_free(e);
    }
    pl_free(estack);
    set_estack(NULL);
}

void errors_push_state() {
    pl* estack;
    err_t* now;
    err_t* snapshot;
    // make sure the stack and current state are initialized
    errors_get_state();
    estack = get_estack();
    now = pl_pop(estack);
    snapshot = error_copy(now);
    pl_push(estack, snapshot);
    pl_push(estack, now);
    //free(snapshot); //# Modified by Robert Lancaster for the StellarSolver Internal Library, preventing leaks, the push copies the object
    //This was in fact flagged as a leak, but deleting it may have caused crashes.
}

void errors_pop_state() {
    pl* estack = get_estack();
    if (estack) {
        err_t* now = pl_pop(estack);
        error_free(now);
    }
}

void errors_print_stack(FILE* f) {
    error_print_stack(errors_get_state(), f);
}

void report_error(const char* modfile, int modline,
                  const char* modfunc, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    error_reportv(errors_get_state(), modfile, modline, modfunc, fmt, va);
    va_end(va);
}

void report_errno() {
    char errbuf[128]; //# Modified for the StellarSolver Internal Library for thread safety
    portable_strerror_r(errno, errbuf, sizeof(errbuf)); //# Modified for the StellarSolver Internal Library for thread safety
    error_report(errors_get_state(), "system", -1, "", "%s", errbuf);
}

err_t* error_new() {
    err_t* e = calloc(1, sizeof(err_t));
    e->errstack = bl_new(4, sizeof(errentry_t));
    return e;
}

void error_free(err_t* e) {
    if (!e) return;
    error_stack_clear(e);
    bl_free(e->errstack);
    free(e);
}

int error_nerrs(const err_t* e) {
    return error_stack_N_entries(e);
}

char* error_get_errstr(const err_t* e, int i) {
    errentry_t* ee = error_stack_get_entry(e, i);
    return ee->str;
}

void error_report(err_t* e, const char* module, int line, const char* func, 
                  const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    error_reportv(errors_get_state(), module, line, func, fmt, va);
    va_end(va);
}

void error_reportv(err_t* e, const char* module, int line,
                   const char* func, const char* fmt, va_list va) {
    if (e->print) {
        if (line == -1)
            debug("%s: ", module); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        else
            debug("%s:%i:%s: ", module, line, func); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        debug(fmt, va); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        debug("\n"); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
    }
    if (e->save) {
        error_stack_add_entryv(e, module, line, func, fmt, va);
    }
    if (e->errfunc) {
        e->errfunc(e->baton, e, module, line, func, fmt, va);
    }
}

void error_print_stack(err_t* e, FILE* f) {
    int i;
    anbool first=TRUE;
    for (i=error_stack_N_entries(e)-1; i>=0; i--) {
        errentry_t* ee = error_stack_get_entry(e, i);
        if (!first)
            debug(" "); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        if (ee->line >= 0) {
            debug("%s:%i:%s %s\n", ee->file, ee->line, ee->func, ee->str); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        } else {
            debug("%s:%s %s\n", ee->file, ee->func, ee->str); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        }
        first = FALSE;
    }
}

char* error_get_errs(err_t* e, const char* separator) {
    sl* errs = sl_new(4);
    int i,N;
    char* rtn;
    N = error_stack_N_entries(e);
    for (i=0; i<N; i++) {
        errentry_t* ee = error_stack_get_entry(e, i);
        sl_append(errs, ee->str);
    }
    rtn = sl_join_reverse(errs, separator);
    sl_free2(errs);
    return rtn;
}
//# Modified by Robert Lancaster for the StellarSolver Internal Library
/**
void errors_regex_error(int errcode, const regex_t* re) {
    char str[256];
    regerror(errcode, re, str, sizeof(str));
    error_report(errors_get_state(), "regex", -1, NULL, "%s", str);
}
**/
void error_stack_add_entryv(err_t* e, const char* file, int line, const char* func, const char* format, va_list va) {
    char* str;
    if (vasprintf(&str, format, va) == -1) {
        debug("vasprintf failed with format string: \"%s\"\n", format); //# Modified by Robert Lancaster for the StellarSolver Internal Library for logging
        return;
    }
    error_stack_add_entry(e, file, line, func, str);
    free(str);
}

void error_stack_add_entry(err_t* e, const char* file, int line, const char* func, const char* str) {
    errentry_t ee;
    ee.file = strdup_safe(file);
    ee.line = line;
    ee.func = strdup_safe(func);
    ee.str = strdup_safe(str);
    bl_append(e->errstack, &ee);
}

errentry_t* error_stack_get_entry(const err_t* e, int i) {
    return bl_access(e->errstack, i);
}

int error_stack_N_entries(const err_t* e) {
    return bl_size(e->errstack);
}

void error_stack_clear(err_t* e) {
    int i;
    int N = bl_size(e->errstack);
    for (i=0; i<N; i++) {
        errentry_t* ee = bl_access(e->errstack, i);
        free(ee->file);
        free(ee->func);
        free(ee->str);
    }
    bl_remove_all(e->errstack);
}
