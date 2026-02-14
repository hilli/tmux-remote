#ifndef NABTOSHELL_PATTERN_CONFIG_H_
#define NABTOSHELL_PATTERN_CONFIG_H_

#include <stddef.h>

#include "nabtoshell_prompt.h"

typedef struct {
    char* label;
    char* keys;
} nabtoshell_pattern_action;

typedef struct {
    char* keys;
} nabtoshell_pattern_action_template;

typedef struct {
    char* id;
    nabtoshell_prompt_type type;
    char* prompt_regex;
    char* option_regex;
    nabtoshell_pattern_action* actions;
    int action_count;
    nabtoshell_pattern_action_template* action_template;
    int max_scan_lines;
} nabtoshell_pattern_definition;

typedef struct {
    char* id;
    char* name;
    nabtoshell_pattern_definition* patterns;
    int pattern_count;
} nabtoshell_agent_config;

typedef struct {
    int version;
    nabtoshell_agent_config* agents;
    int agent_count;
} nabtoshell_pattern_config;

nabtoshell_pattern_config* nabtoshell_pattern_config_parse(const char* json,
                                                           size_t json_len);

void nabtoshell_pattern_config_free(nabtoshell_pattern_config* config);

const nabtoshell_agent_config* nabtoshell_pattern_config_find_agent(
    const nabtoshell_pattern_config* config,
    const char* agent_id);

#endif
