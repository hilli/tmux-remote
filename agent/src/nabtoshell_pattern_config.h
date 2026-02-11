#ifndef NABTOSHELL_PATTERN_CONFIG_H
#define NABTOSHELL_PATTERN_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PATTERN_TYPE_YES_NO,
    PATTERN_TYPE_NUMBERED_MENU,
    PATTERN_TYPE_ACCEPT_REJECT
} nabtoshell_pattern_type;

typedef struct {
    char *label;
    char *keys;
} nabtoshell_pattern_action;

typedef struct {
    char *keys;  // e.g. "{number}\n"
} nabtoshell_pattern_action_template;

typedef struct {
    char *id;
    nabtoshell_pattern_type type;
    char *regex;
    bool multi_line;
    nabtoshell_pattern_action *actions;
    int action_count;
    nabtoshell_pattern_action_template *action_template;  // for numbered_menu
} nabtoshell_pattern_definition;

typedef struct {
    char *id;    // key in the agents map
    char *name;
    nabtoshell_pattern_definition *patterns;
    int pattern_count;
} nabtoshell_agent_config;

typedef struct {
    int version;
    nabtoshell_agent_config *agents;
    int agent_count;
} nabtoshell_pattern_config;

// Parse JSON data into config. Returns NULL on failure. Caller must free with _free().
nabtoshell_pattern_config *nabtoshell_pattern_config_parse(const char *json, size_t json_len);
void nabtoshell_pattern_config_free(nabtoshell_pattern_config *config);

// Find agent by id. Returns NULL if not found.
const nabtoshell_agent_config *nabtoshell_pattern_config_find_agent(
    const nabtoshell_pattern_config *config, const char *agent_id);

#endif
