#include "tmuxremote_client_log.h"

#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

FILE* g_log_file = NULL;
const char* g_nabto_log_level = NULL;

static void log_timestamp(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm tm;
    localtime_r(&tv.tv_sec, &tm);
    fprintf(g_log_file, "[%02d:%02d:%02d.%03d] ",
            tm.tm_hour, tm.tm_min, tm.tm_sec, (int)(tv.tv_usec / 1000));
}

bool tmuxremote_log_open(const char* path)
{
    g_log_file = fopen(path, "a");
    if (g_log_file == NULL) {
        fprintf(stderr, "Failed to open log file: %s\n", path);
        return false;
    }
    /* Line-buffered so messages appear promptly */
    setvbuf(g_log_file, NULL, _IOLBF, 0);
    return true;
}

void tmuxremote_log_close(void)
{
    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

void tmuxremote_log(const char* fmt, ...)
{
    if (g_log_file == NULL) {
        return;
    }
    log_timestamp();
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log_file, fmt, ap);
    va_end(ap);
    fprintf(g_log_file, "\n");
}

void tmuxremote_nabto_log_callback(const NabtoClientLogMessage* message, void* data)
{
    (void)data;
    if (g_log_file == NULL) {
        return;
    }
    log_timestamp();
    fprintf(g_log_file, "[NABTO %s] %s: %s\n",
            message->severityString ? message->severityString : "?",
            message->module ? message->module : "core",
            message->message ? message->message : "");
}
