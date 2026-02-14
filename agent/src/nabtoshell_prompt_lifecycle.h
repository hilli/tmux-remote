#ifndef NABTOSHELL_PROMPT_LIFECYCLE_H_
#define NABTOSHELL_PROMPT_LIFECYCLE_H_

#include <stdbool.h>
#include <stdint.h>

#include "nabtoshell_prompt.h"
#include "nabtoshell_prompt_rules.h"

#define NABTOSHELL_PROMPT_ABSENCE_SNAPSHOTS 1

typedef void (*nabtoshell_prompt_lifecycle_callback)(
    nabtoshell_prompt_event_type type,
    const nabtoshell_prompt_instance* instance,
    const char* instance_id,
    void* user_data);

typedef struct {
    nabtoshell_prompt_instance active;
    bool has_active;

    int absence_snapshots;
    uint64_t last_sequence;

    char resolved_instance_id[NABTOSHELL_PROMPT_INSTANCE_ID_MAX];
    bool suppress_resolved;

    nabtoshell_prompt_lifecycle_callback callback;
    void* callback_user_data;
} nabtoshell_prompt_lifecycle;

void nabtoshell_prompt_lifecycle_init(nabtoshell_prompt_lifecycle* lifecycle);

void nabtoshell_prompt_lifecycle_free(nabtoshell_prompt_lifecycle* lifecycle);

void nabtoshell_prompt_lifecycle_set_callback(nabtoshell_prompt_lifecycle* lifecycle,
                                              nabtoshell_prompt_lifecycle_callback callback,
                                              void* user_data);

void nabtoshell_prompt_lifecycle_process(nabtoshell_prompt_lifecycle* lifecycle,
                                         const nabtoshell_prompt_candidate* candidate,
                                         uint64_t snapshot_sequence);

void nabtoshell_prompt_lifecycle_resolve(nabtoshell_prompt_lifecycle* lifecycle,
                                         const char* instance_id);

bool nabtoshell_prompt_lifecycle_copy_active(
    nabtoshell_prompt_lifecycle* lifecycle,
    nabtoshell_prompt_instance* out_instance);

#endif
