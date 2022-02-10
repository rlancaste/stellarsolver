/*
 # This file is part of the Astrometry.net suite.
 # Licensed under a 3-clause BSD style license - see LICENSE
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32 //# Modified by Jasem Mutlaq for the StellarSolver Internal Library
#include <unistd.h>
#endif

#ifdef _WIN32 //# Modified by Robert Lancaster for the StellarSolver Internal Library
//From http://asprintf.insanecoding.org
#define insane_free(ptr) { free(ptr); ptr = 0; }
#include <limits.h>
int vasprintf(char **strp, const char *fmt, va_list ap)
{
  int r = -1, size = _vscprintf(fmt, ap);

  if ((size >= 0) && (size < INT_MAX))
  {
    *strp = (char *)malloc(size+1); //+1 for null
    if (*strp)
    {
      r = vsnprintf(*strp, size+1, fmt, ap);  //+1 for null
      if ((r < 0) || (r > size))
      {
        insane_free(*strp);
        r = -1;
      }
    }
  }
  else { *strp = 0; }

  return(r);
}
#endif

#include <assert.h>

#include "log.h"
#include "an-thread.h"
#include "tic.h"

static int g_thread_specific = 0;
static log_t g_logger;
int astrometryLogToFile = 0; //# Modified by Robert Lancaster for the StellarSolver Internal Library

void log_set_thread_specific() {
    g_thread_specific = 1;
}

static void* logts_init_key(void* user) {
    log_t* l = malloc(sizeof(log_t));
    if (user)
        memcpy(l, user, sizeof(log_t));
    return l;
}
#define TSNAME logts
#include "thread-specific.inc"

static log_t* get_logger() {
#ifndef _MSC_VER //# Modified by Robert Lancaster for the StellarSolver Internal Library
    if (g_thread_specific)
        return logts_get_key(&g_logger);
#endif
    return &g_logger;
}

void log_init_structure(log_t* logger, enum log_level level) {
    logger->level = level;
    logger->f = stdout;
    logger->timestamp = FALSE;
    logger->t0 = timenow();
    logger->logfunc = NULL;
    logger->baton = NULL;
}

void log_init(enum log_level level) {
    log_init_structure(get_logger(), level);
}

void log_set_level(enum log_level level) {
    get_logger()->level = level;
}

void log_set_timestamp(anbool b) {
    get_logger()->timestamp = b;
}

void log_to(FILE* fid) {
    get_logger()->f = fid;
    astrometryLogToFile = 1; //# Modified by Robert Lancaster for the StellarSolver Internal Library
}

void log_to_fd(int fd) {
    // MEMLEAK
    FILE* fid = fdopen(fd, "a");
    log_to(fid);
}

void log_use_function(logfunc_t func, void* baton) {
    log_t* l = get_logger();
    l->logfunc = func;
    l->baton = baton;
}
/* //# Modified by Robert Lancaster for the StellarSolver Internal Library, this method was not used.
log_t* log_create(enum log_level level) {
    log_t* logger = calloc(1, sizeof(log_t));
    return logger;
}
*/
void log_free(log_t* log) {
    assert(log);
    free(log);
}


/* Modified by Robert Lancaster for the StellarSolver Internal Library
* These methods replace the former logging functions.
* There were several reasons to do this:
* 1. To get the log text back to stellarsolver as a character string
* 2. To get rid of the errors caused by implicitly declared functions due to the templates
* 3. Because on an armhf system, the former logging functions were not putting the correct variable values in the logs
*/

void logerr(const char* text, ...){
    va_list va;
    va_start(va, text);
    log_this(text, LOG_ERROR, va);
    va_end(va);
}
void logmsg(const char* text, ...){
    va_list va;
    va_start(va, text);
    log_this(text, LOG_MSG, va);
    va_end(va);
}
void logverb(const char* text, ...){
    va_list va;
    va_start(va, text);
    log_this(text, LOG_VERB, va);
    va_end(va);
}
void debug(const char* text, ...){
    va_list va;
    va_start(va, text);
    log_this(text, LOG_ALL, va);
    va_end(va);
}
void logdebug(const char* text, ...){
    va_list va;
    va_start(va, text);
    log_this(text, LOG_ALL, va);
    va_end(va);
}
void loglevel(enum log_level level, const char* text, ...){
    va_list va;
    va_start(va, text);
    log_this(text, level, va);
    va_end(va);
}

void log_this(const char* text, enum log_level level, va_list va){
    const log_t* logger = get_logger();
    if (level > logger->level)
        return;

    if (logger->f && astrometryLogToFile == 1) {
        if (logger->timestamp)
            fprintf(logger->f, "[ %.3f] ", timenow() - logger->t0);
        vfprintf(logger->f, text, va);
        fflush(logger->f);
    }
    else{
        char *formatted = NULL;
        vasprintf(&formatted, text, va);
        logToStellarSolver(formatted);
        free(formatted);
    }
}

// This is the end of the added functions.

int log_get_level() {
    return get_logger()->level;
}

FILE* log_get_fid() {
    return get_logger()->f;
}




