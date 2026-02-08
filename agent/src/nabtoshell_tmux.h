#ifndef NABTOSHELL_TMUX_H_
#define NABTOSHELL_TMUX_H_

#include <stdbool.h>
#include <stdint.h>

#define NABTOSHELL_TMUX_MAX_SESSIONS 32

struct nabtoshell_tmux_session {
    char name[64];
    uint16_t cols;
    uint16_t rows;
    int attached;
};

struct nabtoshell_tmux_list {
    struct nabtoshell_tmux_session sessions[NABTOSHELL_TMUX_MAX_SESSIONS];
    int count;
};

bool nabtoshell_tmux_list_sessions(struct nabtoshell_tmux_list* list);
bool nabtoshell_tmux_session_exists(const char* name);
bool nabtoshell_tmux_create_session(const char* name, uint16_t cols,
                                    uint16_t rows, const char* command);
bool nabtoshell_tmux_validate_session_name(const char* name);

#endif
