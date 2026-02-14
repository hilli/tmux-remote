#ifndef NABTOSHELL_PROMPT_RULES_H_
#define NABTOSHELL_PROMPT_RULES_H_

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <stdbool.h>

#include "nabtoshell_pattern_config.h"
#include "nabtoshell_prompt.h"
#include "nabtoshell_terminal_state.h"

typedef struct {
    char* pattern_id;
    nabtoshell_prompt_type pattern_type;
    char* prompt;
    nabtoshell_prompt_action actions[NABTOSHELL_PROMPT_MAX_ACTIONS];
    int action_count;
    int anchor_row;
} nabtoshell_prompt_candidate;

typedef struct {
    char* pattern_id;
    nabtoshell_prompt_type pattern_type;
    int max_scan_lines;

    pcre2_code* prompt_regex;
    pcre2_code* option_regex;

    nabtoshell_prompt_action* static_actions;
    int static_action_count;
    char* action_template_keys;
} nabtoshell_compiled_prompt_rule;

typedef struct {
    nabtoshell_compiled_prompt_rule* rules;
    int count;
} nabtoshell_prompt_ruleset;

void nabtoshell_prompt_candidate_free(nabtoshell_prompt_candidate* candidate);

bool nabtoshell_prompt_candidate_to_instance(const nabtoshell_prompt_candidate* candidate,
                                             nabtoshell_prompt_instance* instance);

void nabtoshell_prompt_ruleset_init(nabtoshell_prompt_ruleset* ruleset);

void nabtoshell_prompt_ruleset_free(nabtoshell_prompt_ruleset* ruleset);

bool nabtoshell_prompt_ruleset_load(nabtoshell_prompt_ruleset* ruleset,
                                    const nabtoshell_pattern_definition* definitions,
                                    int definition_count);

bool nabtoshell_prompt_ruleset_match(const nabtoshell_prompt_ruleset* ruleset,
                                     const nabtoshell_terminal_snapshot* snapshot,
                                     nabtoshell_prompt_candidate* out_candidate);

#endif
