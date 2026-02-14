#include "nabtoshell_prompt_detector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEWLINE "\n"

static void lifecycle_forwarder(nabtoshell_prompt_event_type type,
                                const nabtoshell_prompt_instance* instance,
                                const char* instance_id,
                                void* user_data)
{
    nabtoshell_prompt_detector* detector = user_data;
    if (detector->callback != NULL) {
        detector->callback(type,
                           instance,
                           instance_id,
                           detector->callback_user_data);
    }
}

static const nabtoshell_agent_config* default_agent(
    const nabtoshell_pattern_config* config)
{
    if (config == NULL) {
        return NULL;
    }

    for (int i = 0; i < config->agent_count; i++) {
        if (config->agents[i].pattern_count > 0 &&
            config->agents[i].id != NULL) {
            return &config->agents[i];
        }
    }

    return NULL;
}

static void reload_rules_locked(nabtoshell_prompt_detector* detector)
{
    nabtoshell_prompt_ruleset_load(&detector->ruleset, NULL, 0);

    if (detector->config == NULL || detector->active_agent == NULL) {
        return;
    }

    const nabtoshell_agent_config* agent =
        nabtoshell_pattern_config_find_agent(detector->config, detector->active_agent);
    if (agent == NULL) {
        return;
    }

    nabtoshell_prompt_ruleset_load(&detector->ruleset,
                                   agent->patterns,
                                   agent->pattern_count);
}

void nabtoshell_prompt_detector_init(nabtoshell_prompt_detector* detector,
                                     int rows,
                                     int cols)
{
    memset(detector, 0, sizeof(*detector));

    nabtoshell_terminal_state_init(&detector->terminal_state, rows, cols);
    nabtoshell_prompt_ruleset_init(&detector->ruleset);
    nabtoshell_prompt_lifecycle_init(&detector->lifecycle);
    nabtoshell_prompt_lifecycle_set_callback(&detector->lifecycle,
                                             lifecycle_forwarder,
                                             detector);

    pthread_mutex_init(&detector->mutex, NULL);
}

void nabtoshell_prompt_detector_free(nabtoshell_prompt_detector* detector)
{
    if (detector == NULL) {
        return;
    }

    pthread_mutex_lock(&detector->mutex);
    free(detector->active_agent);
    detector->active_agent = NULL;

    nabtoshell_prompt_lifecycle_free(&detector->lifecycle);
    nabtoshell_prompt_ruleset_free(&detector->ruleset);
    nabtoshell_terminal_state_free(&detector->terminal_state);
    pthread_mutex_unlock(&detector->mutex);

    pthread_mutex_destroy(&detector->mutex);
}

void nabtoshell_prompt_detector_set_callback(nabtoshell_prompt_detector* detector,
                                             nabtoshell_prompt_detector_callback callback,
                                             void* user_data)
{
    pthread_mutex_lock(&detector->mutex);
    detector->callback = callback;
    detector->callback_user_data = user_data;
    pthread_mutex_unlock(&detector->mutex);
}

void nabtoshell_prompt_detector_load_config(nabtoshell_prompt_detector* detector,
                                            const nabtoshell_pattern_config* config)
{
    pthread_mutex_lock(&detector->mutex);

    detector->config = config;

    if (detector->active_agent == NULL && config != NULL) {
        const nabtoshell_agent_config* agent = default_agent(config);
        if (agent != NULL && agent->id != NULL) {
            detector->active_agent = strdup(agent->id);
        }
    }

    reload_rules_locked(detector);
    pthread_mutex_unlock(&detector->mutex);
}

void nabtoshell_prompt_detector_select_agent(nabtoshell_prompt_detector* detector,
                                             const char* agent_id)
{
    pthread_mutex_lock(&detector->mutex);

    free(detector->active_agent);
    detector->active_agent = agent_id ? strdup(agent_id) : NULL;

    reload_rules_locked(detector);
    pthread_mutex_unlock(&detector->mutex);
}

void nabtoshell_prompt_detector_feed(nabtoshell_prompt_detector* detector,
                                     const uint8_t* data,
                                     size_t len)
{
    if (detector == NULL || data == NULL || len == 0) {
        return;
    }

    pthread_mutex_lock(&detector->mutex);

    nabtoshell_terminal_state_feed(&detector->terminal_state, data, len);

    nabtoshell_terminal_snapshot snapshot;
    if (!nabtoshell_terminal_state_snapshot(&detector->terminal_state, &snapshot)) {
        pthread_mutex_unlock(&detector->mutex);
        return;
    }

    nabtoshell_prompt_candidate candidate;
    bool has_candidate = nabtoshell_prompt_ruleset_match(
        &detector->ruleset,
        &snapshot,
        &candidate);

    if (has_candidate) {
        nabtoshell_prompt_lifecycle_process(&detector->lifecycle,
                                            &candidate,
                                            snapshot.sequence);
        nabtoshell_prompt_candidate_free(&candidate);
    } else {
        nabtoshell_prompt_lifecycle_process(&detector->lifecycle,
                                            NULL,
                                            snapshot.sequence);
    }

    nabtoshell_terminal_snapshot_free(&snapshot);

    pthread_mutex_unlock(&detector->mutex);
}

void nabtoshell_prompt_detector_resolve(nabtoshell_prompt_detector* detector,
                                        const char* instance_id,
                                        const char* decision,
                                        const char* keys)
{
    (void)decision;
    (void)keys;

    if (detector == NULL || instance_id == NULL || instance_id[0] == '\0') {
        return;
    }

    pthread_mutex_lock(&detector->mutex);
    nabtoshell_prompt_lifecycle_resolve(&detector->lifecycle, instance_id);
    pthread_mutex_unlock(&detector->mutex);
}

nabtoshell_prompt_instance* nabtoshell_prompt_detector_copy_active(
    nabtoshell_prompt_detector* detector)
{
    if (detector == NULL) {
        return NULL;
    }

    nabtoshell_prompt_instance* copy = calloc(1, sizeof(*copy));
    if (copy == NULL) {
        return NULL;
    }

    pthread_mutex_lock(&detector->mutex);
    bool ok = nabtoshell_prompt_lifecycle_copy_active(&detector->lifecycle, copy);
    pthread_mutex_unlock(&detector->mutex);

    if (!ok) {
        nabtoshell_prompt_instance_free(copy);
        free(copy);
        return NULL;
    }

    return copy;
}
