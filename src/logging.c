#include "debug.h"

#include "settings.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

int utox_verbosity() {
    return settings.verbose;
}

#ifndef __ANDROID__ // Android needs to provide it's own logging functions

void debug(const char *fmt, ...){
    if (!settings.debug_file) {
        return;
    }

    va_list list;

#define LOGGING_FMT_NEW_STR_MAX_LEN 1000
    char logging_fmt_str[LOGGING_FMT_NEW_STR_MAX_LEN];
    memset(logging_fmt_str, 0, LOGGING_FMT_NEW_STR_MAX_LEN);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm = *localtime(&tv.tv_sec);

#define LOGGING_TIMESTAMP_STR_MAX_LEN 26
    // 2019-08-03 17:01:04.440494 --> 4+1+2+1+2+1+2+1+2+1+2+1+6 = 26
    char timestamp_str[LOGGING_TIMESTAMP_STR_MAX_LEN + 1];
    memset(timestamp_str, 0, (LOGGING_TIMESTAMP_STR_MAX_LEN + 1));
    snprintf(timestamp_str, LOGGING_TIMESTAMP_STR_MAX_LEN, "%04d-%02d-%02d %02d:%02d:%02d.%06ld", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec);

    va_start(list, fmt);

    snprintf(logging_fmt_str, (LOGGING_FMT_NEW_STR_MAX_LEN - 1), "%s:%s", timestamp_str, fmt);
    vfprintf(settings.debug_file, logging_fmt_str, list);

    va_end(list);

    #ifdef __WIN32__
    fflush(settings.debug_file);
    #endif
}

#endif
