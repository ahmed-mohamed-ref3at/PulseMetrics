/**
 * PulseMetrics - Systems Programming Project
 * Module: Logger
 * File: core/src/logger.c
 */

#include "logger.h"
#include <windows.h>
#include <stdarg.h>
#include <string.h>

static LogLevel s_current_level = LOG_LEVEL_INFO;
static FILE *s_log_stream = NULL;
static CRITICAL_SECTION s_log_lock;
static bool s_lock_initialized = false;

static void ensure_lock_initialized(void) {
    if (!s_lock_initialized) {
        InitializeCriticalSection(&s_log_lock);
        s_lock_initialized = true;
    }
}

void logger_init(LogLevel level, FILE *stream) {
    ensure_lock_initialized();
    EnterCriticalSection(&s_log_lock);
    s_current_level = level;
    s_log_stream = (stream != NULL) ? stream : stderr;
    LeaveCriticalSection(&s_log_lock);
}

void logger_set_level(LogLevel level) {
    ensure_lock_initialized();
    EnterCriticalSection(&s_log_lock);
    s_current_level = level;
    LeaveCriticalSection(&s_log_lock);
}

LogLevel logger_level_from_string(const char *str) {
    if (str == NULL) return LOG_LEVEL_INFO;
    if (_stricmp(str, "debug") == 0) return LOG_LEVEL_DEBUG;
    if (_stricmp(str, "info") == 0)  return LOG_LEVEL_INFO;
    if (_stricmp(str, "warn") == 0)  return LOG_LEVEL_WARN;
    if (_stricmp(str, "warning") == 0) return LOG_LEVEL_WARN;
    if (_stricmp(str, "error") == 0) return LOG_LEVEL_ERROR;
    if (_stricmp(str, "none") == 0)  return LOG_LEVEL_NONE;
    return LOG_LEVEL_INFO;
}

static const char *level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "LOG  ";
    }
}

static const char *extract_filename(const char *path) {
    if (path == NULL) return "unknown";
    const char *p1 = strrchr(path, '/');
    const char *p2 = strrchr(path, '\\');
    const char *last = (p1 > p2) ? p1 : p2;
    return (last != NULL) ? last + 1 : path;
}

void logger_log(LogLevel level, const char *file, int line, const char *fmt, ...) {
    ensure_lock_initialized();

    if (level < s_current_level || level >= LOG_LEVEL_NONE) {
        return;
    }

    EnterCriticalSection(&s_log_lock);

    FILE *out = (s_log_stream != NULL) ? s_log_stream : stderr;

    /* Retrieve local wall-clock time with millisecond precision */
    SYSTEMTIME st;
    GetLocalTime(&st);

    /* Formatted header */
    fprintf(out, "[%04d-%02d-%02d %02d:%02d:%02d.%03d] [%s] [%s:%d] ",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
            level_to_string(level),
            extract_filename(file), line);

    /* Print variable arguments */
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fprintf(out, "\n");
    fflush(out);

    LeaveCriticalSection(&s_log_lock);
}

