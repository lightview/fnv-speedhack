#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "log.h"
#include "hooks.h"

static FILE* g_log = NULL;

void log_init(const char* dllDir, int enabled)
{
    if (!enabled)
        return;

    char path[MAX_PATH];
    _snprintf(path, sizeof(path), "%s\\fnvspeed.log", dllDir);
    path[sizeof(path) - 1] = 0;

    /* fresh log per game session */
    g_log = fopen(path, "w");
    if (g_log)
        setvbuf(g_log, NULL, _IONBF, 0);
}

void log_write(const char* fmt, ...)
{
    if (!g_log)
        return;

    fprintf(g_log, "[%08lu] ", (unsigned long)hooks_real_time_ms());

    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);

    fputc('\n', g_log);
}

void log_shutdown(void)
{
    if (g_log) {
        fclose(g_log);
        g_log = NULL;
    }
}
