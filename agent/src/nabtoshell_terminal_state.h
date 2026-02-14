#ifndef NABTOSHELL_TERMINAL_STATE_H_
#define NABTOSHELL_TERMINAL_STATE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t sequence;
    int rows;
    int cols;
    int cursor_row;
    int cursor_col;
    bool alt_screen;
    char** lines;
} nabtoshell_terminal_snapshot;

typedef struct {
    uint64_t sequence;
    int rows;
    int cols;
    int cursor_row;
    int cursor_col;
    int saved_row;
    int saved_col;
    bool alt_screen;

    char* cells;

    bool in_escape;
    bool in_csi;
    char csi_buf[128];
    size_t csi_len;
} nabtoshell_terminal_state;

void nabtoshell_terminal_state_init(nabtoshell_terminal_state* state,
                                    int rows,
                                    int cols);

void nabtoshell_terminal_state_free(nabtoshell_terminal_state* state);

void nabtoshell_terminal_state_feed(nabtoshell_terminal_state* state,
                                    const uint8_t* data,
                                    size_t len);

bool nabtoshell_terminal_state_snapshot(const nabtoshell_terminal_state* state,
                                        nabtoshell_terminal_snapshot* snapshot);

void nabtoshell_terminal_snapshot_free(nabtoshell_terminal_snapshot* snapshot);

#endif
