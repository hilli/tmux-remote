#ifndef NABTOSHELL_PATTERN_MATCHER_H
#define NABTOSHELL_PATTERN_MATCHER_H

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <stdbool.h>
#include <stddef.h>

#include "nabtoshell_pattern_config.h"

#define PATTERN_MATCHER_MAX_ACTIONS 16

typedef struct {
    char *label;
    char *keys;
} nabtoshell_resolved_action;

typedef struct {
    char *id;
    nabtoshell_pattern_type pattern_type;
    char *prompt;                // NULL for yes_no / accept_reject
    char *matched_text;
    nabtoshell_resolved_action actions[PATTERN_MATCHER_MAX_ACTIONS];
    int action_count;
    size_t match_position;
} nabtoshell_pattern_match;

typedef struct {
    nabtoshell_pattern_definition *def;
    pcre2_code *regex;
} nabtoshell_compiled_pattern;

typedef struct {
    nabtoshell_compiled_pattern *patterns;
    int pattern_count;
} nabtoshell_pattern_matcher;

void nabtoshell_pattern_matcher_init(nabtoshell_pattern_matcher *m);
void nabtoshell_pattern_matcher_reset(nabtoshell_pattern_matcher *m);

// Compile patterns for a specific agent. Previous patterns are freed.
void nabtoshell_pattern_matcher_load(nabtoshell_pattern_matcher *m,
                                      nabtoshell_pattern_definition *defs,
                                      int count);

// Evaluate patterns against text. First match wins.
// Returns dynamically allocated match (caller must free with _match_free) or NULL.
nabtoshell_pattern_match *nabtoshell_pattern_matcher_match(
    const nabtoshell_pattern_matcher *m,
    const char *text, size_t text_len,
    size_t total_appended);

void nabtoshell_pattern_match_free(nabtoshell_pattern_match *match);

#endif
