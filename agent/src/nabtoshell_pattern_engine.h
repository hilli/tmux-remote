#ifndef NABTOSHELL_PATTERN_ENGINE_H
#define NABTOSHELL_PATTERN_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "nabtoshell_ansi_stripper.h"
#include "nabtoshell_rolling_buffer.h"
#include "nabtoshell_pattern_matcher.h"
#include "nabtoshell_pattern_config.h"

#define PATTERN_ENGINE_BUFFER_CAPACITY  8192
#define PATTERN_ENGINE_MATCH_WINDOW     2000
#define PATTERN_ENGINE_AUTO_DISMISS     80

// Callback when a match changes (new match found, match dismissed, etc.)
// match is NULL when dismissed. Ownership NOT transferred; copy if needed.
typedef void (*nabtoshell_pattern_engine_callback)(
    const nabtoshell_pattern_match *match, void *user_data);

typedef struct {
    nabtoshell_ansi_stripper stripper;
    nabtoshell_rolling_buffer buffer;
    nabtoshell_pattern_matcher matcher;
    nabtoshell_pattern_config *config;

    nabtoshell_pattern_match *active_match;
    char *active_agent;

    bool dismissed;
    bool user_dismissed;
    size_t dismissed_at_position;

    nabtoshell_pattern_engine_callback on_change;
    void *on_change_user_data;
} nabtoshell_pattern_engine;

void nabtoshell_pattern_engine_init(nabtoshell_pattern_engine *e);
void nabtoshell_pattern_engine_free(nabtoshell_pattern_engine *e);
void nabtoshell_pattern_engine_reset(nabtoshell_pattern_engine *e);

void nabtoshell_pattern_engine_load_config(nabtoshell_pattern_engine *e,
                                            nabtoshell_pattern_config *config);

// Select agent by id. Pass NULL to deselect. Re-evaluates buffer on select.
void nabtoshell_pattern_engine_select_agent(nabtoshell_pattern_engine *e,
                                             const char *agent_id);

// Feed raw terminal bytes through the pipeline.
void nabtoshell_pattern_engine_feed(nabtoshell_pattern_engine *e,
                                     const uint8_t *data, size_t len);

// User-initiated dismiss of the current overlay.
void nabtoshell_pattern_engine_dismiss(nabtoshell_pattern_engine *e);

// Set callback for match state changes.
void nabtoshell_pattern_engine_set_callback(nabtoshell_pattern_engine *e,
                                             nabtoshell_pattern_engine_callback cb,
                                             void *user_data);

// Auto-detect agent from initial terminal output.
void nabtoshell_pattern_engine_auto_detect(nabtoshell_pattern_engine *e,
                                            const char *text, size_t len);

#endif
