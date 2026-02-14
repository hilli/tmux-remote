#ifndef NABTOSHELL_PROMPT_DETECTOR_H_
#define NABTOSHELL_PROMPT_DETECTOR_H_

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

#include "nabtoshell_pattern_config.h"
#include "nabtoshell_prompt_lifecycle.h"
#include "nabtoshell_prompt_rules.h"
#include "nabtoshell_terminal_state.h"

typedef void (*nabtoshell_prompt_detector_callback)(
    nabtoshell_prompt_event_type type,
    const nabtoshell_prompt_instance* instance,
    const char* instance_id,
    void* user_data);

typedef struct {
    nabtoshell_terminal_state terminal_state;
    nabtoshell_prompt_ruleset ruleset;
    nabtoshell_prompt_lifecycle lifecycle;

    const nabtoshell_pattern_config* config;
    char* active_agent;

    nabtoshell_prompt_detector_callback callback;
    void* callback_user_data;

    pthread_mutex_t mutex;
} nabtoshell_prompt_detector;

void nabtoshell_prompt_detector_init(nabtoshell_prompt_detector* detector,
                                     int rows,
                                     int cols);

void nabtoshell_prompt_detector_free(nabtoshell_prompt_detector* detector);

void nabtoshell_prompt_detector_set_callback(nabtoshell_prompt_detector* detector,
                                             nabtoshell_prompt_detector_callback callback,
                                             void* user_data);

void nabtoshell_prompt_detector_load_config(nabtoshell_prompt_detector* detector,
                                            const nabtoshell_pattern_config* config);

void nabtoshell_prompt_detector_select_agent(nabtoshell_prompt_detector* detector,
                                             const char* agent_id);

void nabtoshell_prompt_detector_feed(nabtoshell_prompt_detector* detector,
                                     const uint8_t* data,
                                     size_t len);

void nabtoshell_prompt_detector_resolve(nabtoshell_prompt_detector* detector,
                                        const char* instance_id,
                                        const char* decision,
                                        const char* keys);

nabtoshell_prompt_instance* nabtoshell_prompt_detector_copy_active(
    nabtoshell_prompt_detector* detector);

#endif
