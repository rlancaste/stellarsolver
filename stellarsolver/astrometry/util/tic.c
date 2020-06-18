/*
 # This file is part of the Astrometry.net suite.
 # Licensed under a 3-clause BSD style license - see LICENSE
 */

#include <time.h>

#ifndef _WIN32 //# Modified by Robert Lancaster for the StellarSolver Internal Library
#include <sys/resource.h>
#include <sys/time.h>
#else
#include <Windows.h>
#endif
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "tic.h"
#include "errors.h"
#include "log.h"

static time_t starttime;
static double starttime2;
static double startutime, startstime;

#ifdef _MSC_VER //# Modified by Robert Lancaster for the StellarSolver Internal Library
//Source: https://stackoverflow.com/questions/10905892/equivalent-of-gettimeday-for-windows
//Marked as a "Free Implementation by the author of the post.
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t EPOCH = ((uint64_t) 116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime( &system_time );
    SystemTimeToFileTime( &system_time, &file_time );
    time =  ((uint64_t)file_time.dwLowDateTime )      ;
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec  = (long) ((time - EPOCH) / 10000000L);
    tp->tv_usec = (long) (system_time.wMilliseconds * 1000);
    return 0;
}
#endif

double timenow() {
    struct timeval tv;
    if (gettimeofday(&tv, NULL)) {
        ERROR("Failed to get time of day");
        return -1.0;
    }
    return (double)(tv.tv_sec - 3600*24*365*30) + tv.tv_usec * 1e-6;
}

double millis_between(struct timeval* tv1, struct timeval* tv2) {
    return
        (tv2->tv_usec - tv1->tv_usec)*1e-3 +
        (tv2->tv_sec  - tv1->tv_sec )*1e3;
}

void tic() {
    starttime = time(NULL);
    starttime2 = timenow();
    if (get_resource_stats(&startutime, &startstime, NULL)) {
        ERROR("Failed to get_resource_stats()");
        return;
    }
}

int get_resource_stats(double* p_usertime, double* p_systime, long* p_maxrss) {
#ifndef _WIN32   //# Modified by Robert Lancaster for the StellarSolver Internal Library
    //Getting the rusage would be much more difficult on windows
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage)) {
        SYSERROR("Failed to get resource stats (getrusage)");
        return 1;
    }
    if (p_usertime) {
        *p_usertime = usage.ru_utime.tv_sec + 1e-6 * usage.ru_utime.tv_usec;
    }
    if (p_systime) {
        *p_systime = usage.ru_stime.tv_sec + 1e-6 * usage.ru_stime.tv_usec;
    }
    if (p_maxrss) {
        *p_maxrss = usage.ru_maxrss;
    }
    return 0;
#else  //# Modified by Robert Lancaster for the StellarSolver Internal Library
   return 1;
#endif
}

void toc() {
    double utime, stime;
    long rss;
    //int dtime;
    double dtime2;
    //time_t endtime = time(NULL);
    //dtime = (int)(endtime - starttime);
    dtime2 = timenow() - starttime2;
    if (get_resource_stats(&utime, &stime, &rss)) {
        ERROR("Failed to get_resource_stats()");
        return;
    }
    logmsg("Used %g s user, %g s system (%g s total), %g s wall time since last check\n",
           utime-startutime, stime-startstime, (utime + stime)-(startutime+startstime), dtime2);
}
