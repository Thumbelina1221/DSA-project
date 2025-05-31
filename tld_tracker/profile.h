#ifndef LOG_DURATION_H
#define LOG_DURATION_H

#include <stdio.h>
#include <time.h>

typedef struct {
    const char* message;
    clock_t start;
} LogDuration;

static inline void log_duration_start(LogDuration* ld, const char* msg) {
    ld->message = msg;
    ld->start = clock();
}

static inline void log_duration_end(LogDuration* ld) {
    clock_t finish = clock();
    double ms = 1000.0 * (finish - ld->start) / CLOCKS_PER_SEC;
    fprintf(stderr, "%s%.0f ms\n", ld->message ? ld->message : "", ms);
}

// Helper for unique variable name
#define LOG_DURATION_VAR_NAME2(x, y) x##y
#define LOG_DURATION_VAR_NAME(x, y) LOG_DURATION_VAR_NAME2(x, y)

// Usage: LOG_DURATION("my code")
//      ... code block ...
//      LOG_DURATION_END
#define LOG_DURATION(msg) \
    LogDuration LOG_DURATION_VAR_NAME(_logduration_, __LINE__); \
    log_duration_start(&LOG_DURATION_VAR_NAME(_logduration_, __LINE__), msg);

#define LOG_DURATION_END \
    log_duration_end(&LOG_DURATION_VAR_NAME(_logduration_, __LINE__))

#endif

