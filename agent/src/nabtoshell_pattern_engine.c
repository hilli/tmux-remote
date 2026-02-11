#include "nabtoshell_pattern_engine.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

void nabtoshell_pattern_engine_init(nabtoshell_pattern_engine *e)
{
    nabtoshell_ansi_stripper_init(&e->stripper);
    nabtoshell_rolling_buffer_init(&e->buffer, PATTERN_ENGINE_BUFFER_CAPACITY);
    nabtoshell_pattern_matcher_init(&e->matcher);
    e->config = NULL;
    e->active_match = NULL;
    e->active_agent = NULL;
    e->dismissed = false;
    e->user_dismissed = false;
    e->dismissed_at_position = 0;
    e->on_change = NULL;
    e->on_change_user_data = NULL;
}

void nabtoshell_pattern_engine_free(nabtoshell_pattern_engine *e)
{
    nabtoshell_pattern_matcher_reset(&e->matcher);
    nabtoshell_rolling_buffer_free(&e->buffer);
    nabtoshell_pattern_match_free(e->active_match);
    free(e->active_agent);
    // config is not owned by engine
}

void nabtoshell_pattern_engine_reset(nabtoshell_pattern_engine *e)
{
    nabtoshell_ansi_stripper_reset(&e->stripper);
    nabtoshell_rolling_buffer_reset(&e->buffer);
    nabtoshell_pattern_match_free(e->active_match);
    e->active_match = NULL;
    e->dismissed = false;
    e->user_dismissed = false;
    e->dismissed_at_position = 0;
}

void nabtoshell_pattern_engine_load_config(nabtoshell_pattern_engine *e,
                                            nabtoshell_pattern_config *config)
{
    e->config = config;
    if (e->active_agent) {
        const nabtoshell_agent_config *ac = nabtoshell_pattern_config_find_agent(config, e->active_agent);
        if (ac) {
            nabtoshell_pattern_matcher_load(&e->matcher, ac->patterns, ac->pattern_count);
        }
    }
}

static void notify_change(nabtoshell_pattern_engine *e)
{
    if (e->on_change) {
        e->on_change(e->active_match, e->on_change_user_data);
    }
}

void nabtoshell_pattern_engine_select_agent(nabtoshell_pattern_engine *e,
                                             const char *agent_id)
{
    free(e->active_agent);
    nabtoshell_pattern_match_free(e->active_match);
    e->active_match = NULL;
    e->dismissed = false;

    if (!agent_id) {
        e->active_agent = NULL;
        nabtoshell_pattern_matcher_reset(&e->matcher);
        notify_change(e);
        return;
    }

    e->active_agent = strdup(agent_id);

    if (e->config) {
        const nabtoshell_agent_config *ac = nabtoshell_pattern_config_find_agent(e->config, agent_id);
        if (ac) {
            nabtoshell_pattern_matcher_load(&e->matcher, ac->patterns, ac->pattern_count);
        } else {
            nabtoshell_pattern_matcher_reset(&e->matcher);
        }
    }

    // Re-evaluate existing buffer
    if (e->buffer.len > 0) {
        size_t tail_len;
        const char *tail = nabtoshell_rolling_buffer_tail(&e->buffer, PATTERN_ENGINE_MATCH_WINDOW, &tail_len);
        e->active_match = nabtoshell_pattern_matcher_match(&e->matcher, tail, tail_len, e->buffer.total_appended);
    }

    notify_change(e);
}

void nabtoshell_pattern_engine_feed(nabtoshell_pattern_engine *e,
                                     const uint8_t *data, size_t len)
{
    // Strip ANSI and buffer regardless of agent selection
    uint8_t stripped[len + 4];
    size_t stripped_len = nabtoshell_ansi_stripper_feed(&e->stripper, data, len, stripped, sizeof(stripped));

    if (stripped_len == 0) return;

    nabtoshell_rolling_buffer_append(&e->buffer, (const char *)stripped, stripped_len);

    if (!e->active_agent) return;

    // Check auto-dismiss for existing match
    if (e->active_match) {
        size_t chars_since_match = e->buffer.total_appended - e->active_match->match_position;
        if (chars_since_match > PATTERN_ENGINE_AUTO_DISMISS) {
            size_t tail_len;
            const char *tail = nabtoshell_rolling_buffer_tail(&e->buffer, PATTERN_ENGINE_MATCH_WINDOW, &tail_len);
            nabtoshell_pattern_match *check = nabtoshell_pattern_matcher_match(&e->matcher, tail, tail_len, e->buffer.total_appended);

            if (!check || strcmp(check->id, e->active_match->id) != 0) {
                e->dismissed = true;
                e->dismissed_at_position = e->buffer.total_appended;
                nabtoshell_pattern_match_free(e->active_match);
                e->active_match = NULL;
                notify_change(e);
            }
            nabtoshell_pattern_match_free(check);
        }
    }

    // Reset dismissed flag if enough new content arrived
    if (e->dismissed) {
        size_t chars_since_dismiss = e->buffer.total_appended - e->dismissed_at_position;
        size_t threshold = e->user_dismissed ? PATTERN_ENGINE_MATCH_WINDOW : PATTERN_ENGINE_AUTO_DISMISS;
        if (chars_since_dismiss > threshold) {
            e->dismissed = false;
            e->user_dismissed = false;
        }
    }

    // Try to find a new match
    if (!e->active_match && !e->dismissed) {
        size_t tail_len;
        const char *tail = nabtoshell_rolling_buffer_tail(&e->buffer, PATTERN_ENGINE_MATCH_WINDOW, &tail_len);
        nabtoshell_pattern_match *new_match = nabtoshell_pattern_matcher_match(&e->matcher, tail, tail_len, e->buffer.total_appended);
        if (new_match) {
            e->active_match = new_match;
            notify_change(e);
        }
    }
}

void nabtoshell_pattern_engine_dismiss(nabtoshell_pattern_engine *e)
{
    e->dismissed = true;
    e->user_dismissed = true;
    e->dismissed_at_position = e->buffer.total_appended;
    nabtoshell_pattern_match_free(e->active_match);
    e->active_match = NULL;
    notify_change(e);
}

void nabtoshell_pattern_engine_set_callback(nabtoshell_pattern_engine *e,
                                             nabtoshell_pattern_engine_callback cb,
                                             void *user_data)
{
    e->on_change = cb;
    e->on_change_user_data = user_data;
}

void nabtoshell_pattern_engine_auto_detect(nabtoshell_pattern_engine *e,
                                            const char *text, size_t len)
{
    if (e->active_agent) return;

    // Simple substring search (case-insensitive)
    char *lower = malloc(len + 1);
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)text[i]);
    }
    lower[len] = '\0';

    if (strstr(lower, "claude code") || strstr(lower, "claude.ai")) {
        nabtoshell_pattern_engine_select_agent(e, "claude-code");
    } else if (strstr(lower, "aider v") || strstr(lower, "aider ")) {
        nabtoshell_pattern_engine_select_agent(e, "aider");
    } else if (strstr(lower, "codex")) {
        nabtoshell_pattern_engine_select_agent(e, "codex");
    }

    free(lower);
}
