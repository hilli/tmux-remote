#ifndef NABTOSHELL_PROMPT_H_
#define NABTOSHELL_PROMPT_H_

#include <stdbool.h>
#include <stdint.h>

#define NABTOSHELL_PROMPT_MAX_ACTIONS 16
#define NABTOSHELL_PROMPT_INSTANCE_ID_MAX 32

typedef enum {
    NABTOSHELL_PROMPT_TYPE_YES_NO = 0,
    NABTOSHELL_PROMPT_TYPE_NUMBERED_MENU = 1,
    NABTOSHELL_PROMPT_TYPE_ACCEPT_REJECT = 2
} nabtoshell_prompt_type;

typedef struct {
    char* label;
    char* keys;
} nabtoshell_prompt_action;

typedef struct {
    char instance_id[NABTOSHELL_PROMPT_INSTANCE_ID_MAX];
    char* pattern_id;
    nabtoshell_prompt_type pattern_type;
    char* prompt;
    nabtoshell_prompt_action actions[NABTOSHELL_PROMPT_MAX_ACTIONS];
    int action_count;
    uint32_t revision;
    int anchor_row;
} nabtoshell_prompt_instance;

typedef enum {
    NABTOSHELL_PROMPT_EVENT_PRESENT = 0,
    NABTOSHELL_PROMPT_EVENT_UPDATE = 1,
    NABTOSHELL_PROMPT_EVENT_GONE = 2
} nabtoshell_prompt_event_type;

void nabtoshell_prompt_instance_reset(nabtoshell_prompt_instance* instance);
void nabtoshell_prompt_instance_free(nabtoshell_prompt_instance* instance);

bool nabtoshell_prompt_instance_copy(const nabtoshell_prompt_instance* src,
                                     nabtoshell_prompt_instance* dst);

bool nabtoshell_prompt_instance_same_semantics(
    const nabtoshell_prompt_instance* a,
    const nabtoshell_prompt_instance* b);

#endif
