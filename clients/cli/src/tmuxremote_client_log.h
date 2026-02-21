#ifndef TMUXREMOTE_CLIENT_LOG_H_
#define TMUXREMOTE_CLIENT_LOG_H_

#include <nabto/nabto_client.h>
#include <stdbool.h>
#include <stdio.h>

/* Global log file handle (NULL means logging disabled) */
extern FILE* g_log_file;

/* Global Nabto SDK log level string (NULL means SDK logging disabled) */
extern const char* g_nabto_log_level;

/* Open the log file. Returns true on success. */
bool tmuxremote_log_open(const char* path);

/* Close the log file. */
void tmuxremote_log_close(void);

/* Write a timestamped message to the log file (printf-style).
   No-op if g_log_file is NULL. */
void tmuxremote_log(const char* fmt, ...);

/* Nabto SDK log callback. Writes SDK log messages to the log file. */
void tmuxremote_nabto_log_callback(const NabtoClientLogMessage* message, void* data);

#endif
