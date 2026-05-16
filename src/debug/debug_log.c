#include "debug_log.h"

#include <signal.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static FILE* g_log = NULL;
static int g_log_fd = -1;

static const char* debug_signal_name(int sig)
{
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGFPE: return "SIGFPE";
        case SIGILL: return "SIGILL";
        case SIGBUS: return "SIGBUS";
        default: return "SIGNAL";
    }
}

static void debug_write_raw(const char* text)
{
    if (!text) return;
    if (g_log_fd >= 0) {
        size_t n = strlen(text);
        if (n > 0) {
            ssize_t written = write(g_log_fd, text, n);
            (void)written;
        }
    }
}

static void debug_signal_handler(int sig)
{
    char buf[128];
    const char* name = debug_signal_name(sig);
    int n = snprintf(buf, sizeof(buf), "\n[FATAL] Caught %s (%d)\n", name, sig);
    if (n > 0) {
        debug_write_raw(buf);
    }
    _exit(128 + sig);
}

void debug_log_init(const char* path)
{
    const char* target = path ? path : "runtime.log";
    g_log = fopen(target, "w");
    if (!g_log) return;
    g_log_fd = fileno(g_log);
    debug_logf("=== Log started ===");
}

void debug_log_close(void)
{
    if (!g_log) return;
    debug_logf("=== Log closed ===");
    fclose(g_log);
    g_log = NULL;
    g_log_fd = -1;
}

void debug_logf(const char* fmt, ...)
{
    time_t now = 0;
    struct tm tmv;
    char stamp[32];
    va_list args;

    if (!g_log || !fmt) return;

    now = time(NULL);
    localtime_r(&now, &tmv);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);

    fprintf(g_log, "[%s] ", stamp);
    va_start(args, fmt);
    vfprintf(g_log, fmt, args);
    va_end(args);
    fputc('\n', g_log);
    fflush(g_log);
}

void debug_log_install_signal_handlers(void)
{
    struct sigaction sa;
    if (getenv("DEBUG_NO_SIGNAL_HANDLERS")) return;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = debug_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}
