#ifndef TMUXREMOTE_INFO_H_
#define TMUXREMOTE_INFO_H_

#include <stdbool.h>
#include <stdio.h>

/* Global silent flag: when true, info_printf is suppressed. */
extern bool tmuxremote_silent;

/* Print informational output (suppressed by --silent). */
#define info_printf(...) do { if (!tmuxremote_silent) printf(__VA_ARGS__); } while (0)

/* Flush stdout if not silent. */
#define info_fflush(f) do { if (!tmuxremote_silent) fflush(f); } while (0)

#endif
