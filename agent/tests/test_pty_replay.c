#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "nabtoshell_pattern_engine.h"
#include "nabtoshell_pattern_config.h"

#ifndef TEST_FIXTURES_DIR
#error "TEST_FIXTURES_DIR must be defined by CMake"
#endif

#define RECORDING_1_PATH TEST_FIXTURES_DIR "/pty-log-1.ptyr"
#define RECORDING_2_PATH TEST_FIXTURES_DIR "/pty-log-2.ptyr"
#define CONFIG_PATH      TEST_FIXTURES_DIR "/patterns.json"

#define MAX_EVENTS 4096

typedef struct {
    bool is_match;   /* true = match, false = dismiss */
    char id[64];
    char matched_text[256];
    int pattern_type;
    int action_count;
    size_t bytes_fed;
} replay_event;

static replay_event events[MAX_EVENTS];
static int event_count = 0;
static size_t total_bytes_fed = 0;

static void replay_callback(const nabtoshell_pattern_match *match, void *user_data)
{
    (void)user_data;
    if (event_count >= MAX_EVENTS) return;

    replay_event *ev = &events[event_count++];
    if (match) {
        ev->is_match = true;
        strncpy(ev->id, match->id ? match->id : "", sizeof(ev->id) - 1);
        ev->id[sizeof(ev->id) - 1] = '\0';
        ev->pattern_type = match->pattern_type;
        ev->action_count = match->action_count;
        if (match->matched_text) {
            strncpy(ev->matched_text, match->matched_text, sizeof(ev->matched_text) - 1);
            ev->matched_text[sizeof(ev->matched_text) - 1] = '\0';
        } else {
            ev->matched_text[0] = '\0';
        }
    } else {
        ev->is_match = false;
        ev->id[0] = '\0';
        ev->matched_text[0] = '\0';
        ev->pattern_type = 0;
        ev->action_count = 0;
    }
    ev->bytes_fed = total_bytes_fed;
}

static void print_timeline(void)
{
    fprintf(stderr, "\n=== PTY Replay Event Timeline (%d events) ===\n", event_count);
    for (int i = 0; i < event_count; i++) {
        replay_event *ev = &events[i];
        if (ev->is_match) {
            char display[204];
            strncpy(display, ev->matched_text, 200);
            display[200] = '\0';
            for (char *p = display; *p; p++) {
                if (*p == '\n' || *p == '\r') *p = '|';
            }
            fprintf(stderr, "[%6zu bytes] MATCH   id=%-30s type=%d actions=%d text=\"%s\"\n",
                    ev->bytes_fed, ev->id, ev->pattern_type, ev->action_count, display);
        } else {
            fprintf(stderr, "[%6zu bytes] DISMISS\n", ev->bytes_fed);
        }
    }
    fprintf(stderr, "=== End Timeline ===\n\n");
}

/* Helper: load a PTYR recording into a malloc'd buffer of frames. */
typedef struct { uint8_t *data; uint32_t len; } ptyr_frame;

static ptyr_frame *load_recording(const char *path, int *out_count)
{
    FILE *rf = fopen(path, "rb");
    ck_assert_msg(rf != NULL, "Failed to open recording: %s", path);

    uint8_t header[16];
    size_t hread = fread(header, 1, 16, rf);
    ck_assert_int_eq(hread, 16);
    ck_assert(memcmp(header, "PTYR", 4) == 0);

    ptyr_frame *frames = NULL;
    int count = 0;
    int cap = 0;

    while (!feof(rf)) {
        uint32_t frame_len_be;
        if (fread(&frame_len_be, 1, 4, rf) < 4) break;
        uint32_t frame_len = ntohl(frame_len_be);
        ck_assert_msg(frame_len > 0 && frame_len <= 65536,
                      "Invalid frame length: %u", frame_len);

        uint8_t *data = malloc(frame_len);
        ck_assert_ptr_nonnull(data);
        size_t n = fread(data, 1, frame_len, rf);
        ck_assert_int_eq(n, frame_len);

        if (count >= cap) {
            cap = cap ? cap * 2 : 256;
            frames = realloc(frames, cap * sizeof(ptyr_frame));
        }
        frames[count].data = data;
        frames[count].len = frame_len;
        count++;
    }

    fclose(rf);
    *out_count = count;
    return frames;
}

static void free_frames(ptyr_frame *frames, int count)
{
    for (int i = 0; i < count; i++) free(frames[i].data);
    free(frames);
}

static nabtoshell_pattern_config *load_config(void)
{
    FILE *cf = fopen(CONFIG_PATH, "r");
    ck_assert_msg(cf != NULL, "Failed to open config: %s", CONFIG_PATH);
    fseek(cf, 0, SEEK_END);
    long csize = ftell(cf);
    fseek(cf, 0, SEEK_SET);
    ck_assert_msg(csize > 0 && csize < 1024 * 1024, "Invalid config size");
    char *json = malloc(csize + 1);
    ck_assert_ptr_nonnull(json);
    size_t cread = fread(json, 1, csize, cf);
    fclose(cf);
    json[cread] = '\0';
    nabtoshell_pattern_config *config = nabtoshell_pattern_config_parse(json, cread);
    free(json);
    ck_assert_msg(config != NULL, "Failed to parse config");
    return config;
}

START_TEST(test_pty_replay_baseline)
{
    /* Baseline replay of pty-log-2.txt without any consume/dismiss injection.
     * Verifies the recording produces the expected match/dismiss timeline. */
    nabtoshell_pattern_config *config = load_config();
    int frame_count;
    ptyr_frame *frames = load_recording(RECORDING_2_PATH, &frame_count);

    nabtoshell_pattern_engine engine;
    nabtoshell_pattern_engine_init(&engine);
    nabtoshell_pattern_engine_load_config(&engine, config);
    nabtoshell_pattern_engine_select_agent(&engine, "claude-code");
    event_count = 0;
    total_bytes_fed = 0;
    nabtoshell_pattern_engine_set_callback(&engine, replay_callback, NULL);

    for (int i = 0; i < frame_count; i++) {
        nabtoshell_pattern_engine_feed(&engine, frames[i].data, frames[i].len);
        total_bytes_fed += frames[i].len;
    }

    fprintf(stderr, "Replayed %d frames, %zu total bytes\n", frame_count, total_bytes_fed);
    print_timeline();

    /* The recording contains multiple "Do you want to proceed?" prompts.
     * Expect at least 2 match events and 1 dismiss. */
    int matches = 0, dismisses = 0;
    for (int i = 0; i < event_count; i++) {
        if (events[i].is_match) matches++;
        else dismisses++;
    }
    ck_assert_msg(matches >= 2, "Expected >= 2 matches, got %d", matches);
    ck_assert_msg(dismisses >= 1, "Expected >= 1 dismiss, got %d", dismisses);

    /* First match should be numbered_prompt with 3 actions. */
    ck_assert(events[0].is_match);
    ck_assert_str_eq(events[0].id, "numbered_prompt");
    ck_assert_int_eq(events[0].action_count, 3);

    free_frames(frames, frame_count);
    nabtoshell_pattern_engine_free(&engine);
    nabtoshell_pattern_config_free(config);
}
END_TEST

START_TEST(test_consume_suppresses_tui_redraws)
{
    /* Reproduces the bug from pty-log-2.txt: user acts on a "Do you want to
     * proceed?" prompt (consume), but the TUI keeps redrawing the same prompt,
     * causing the overlay to reappear. The user had to press "3" six times
     * before the prompt finally went away.
     *
     * With consume() after the first match:
     *   - The same prompt must NOT re-match during TUI redraws (within
     *     MATCH_WINDOW chars of consume). A re-match here is the bug.
     *   - The genuine second prompt (>MATCH_WINDOW chars later) should
     *     still match after consumed_match suppression expires. */
    nabtoshell_pattern_config *config = load_config();
    int frame_count;
    ptyr_frame *frames = load_recording(RECORDING_2_PATH, &frame_count);

    nabtoshell_pattern_engine engine;
    nabtoshell_pattern_engine_init(&engine);
    nabtoshell_pattern_engine_load_config(&engine, config);
    nabtoshell_pattern_engine_select_agent(&engine, "claude-code");
    event_count = 0;
    total_bytes_fed = 0;
    nabtoshell_pattern_engine_set_callback(&engine, replay_callback, NULL);

    bool consumed_first = false;
    size_t consume_position = 0;

    for (int i = 0; i < frame_count; i++) {
        nabtoshell_pattern_engine_feed(&engine, frames[i].data, frames[i].len);
        total_bytes_fed += frames[i].len;

        /* After the first match fires, inject consume(). */
        if (!consumed_first && event_count >= 1 && events[0].is_match) {
            nabtoshell_pattern_engine_consume(&engine);
            consumed_first = true;
            consume_position = total_bytes_fed;
            fprintf(stderr, "[debug-log2] consume at raw=%zu stripped=%zu\n",
                    total_bytes_fed, engine.buffer.total_appended);
        }
    }

    fprintf(stderr, "[debug-log2] end: raw=%zu stripped=%zu\n",
            total_bytes_fed, engine.buffer.total_appended);
    print_timeline();
    ck_assert(consumed_first);

    /* No re-match within MATCH_WINDOW chars after consume.
     * A re-match here means the overlay reappeared from a TUI redraw. */
    for (int i = 0; i < event_count; i++) {
        if (events[i].is_match && events[i].bytes_fed > consume_position) {
            size_t gap = events[i].bytes_fed - consume_position;
            ck_assert_msg(gap > PATTERN_ENGINE_MATCH_WINDOW,
                          "Overlay reappeared %zu bytes after consume (within "
                          "MATCH_WINDOW=%d). TUI redraw was not suppressed.",
                          gap, PATTERN_ENGINE_MATCH_WINDOW);
        }
    }

    /* The genuine second prompt (beyond MATCH_WINDOW) should still match. */
    int late_matches = 0;
    for (int i = 0; i < event_count; i++) {
        if (events[i].is_match &&
            events[i].bytes_fed > consume_position + PATTERN_ENGINE_MATCH_WINDOW) {
            late_matches++;
        }
    }
    ck_assert_msg(late_matches >= 1,
                  "Expected at least 1 match after MATCH_WINDOW expiry, got %d",
                  late_matches);

    free_frames(frames, frame_count);
    nabtoshell_pattern_engine_free(&engine);
    nabtoshell_pattern_config_free(config);
}
END_TEST

START_TEST(test_second_prompt_missed_after_consume)
{
    /* Reproduces the bug from pty-log-1.ptyr. Reporter's steps:
     *
     * 1) Start the app
     * 2) Observe prompt in terminal and a correct overlay
     * 3) Press 3
     * 4) Trigger a new prompt by entering text in terminal
     * 5) Observe prompt in terminal
     * 6) Observe no overlay
     *
     * Root cause: the iOS app calls consume() (step 3), which does NOT
     * send pattern_dismiss to the agent. The agent's active_match from
     * the first prompt is never cleared. When the second prompt appears
     * (step 4-5), the engine stays in "skip-new-match" because
     * active_match is still set, and never fires a callback for the
     * second prompt.
     *
     * Baseline replay (no consume injection) shows only 1 match at
     * ~25986 bytes. The second prompt exists in the recording but the
     * engine misses it because the first active_match blocks detection.
     *
     * With consume() injected after the first match, the engine should
     * detect the second prompt. */
    nabtoshell_pattern_config *config = load_config();
    int frame_count;
    ptyr_frame *frames = load_recording(RECORDING_1_PATH, &frame_count);

    nabtoshell_pattern_engine engine;
    nabtoshell_pattern_engine_init(&engine);
    nabtoshell_pattern_engine_load_config(&engine, config);
    nabtoshell_pattern_engine_select_agent(&engine, "claude-code");
    event_count = 0;
    total_bytes_fed = 0;
    nabtoshell_pattern_engine_set_callback(&engine, replay_callback, NULL);

    bool consumed_first = false;
    size_t consume_position = 0;

    for (int i = 0; i < frame_count; i++) {
        nabtoshell_pattern_engine_feed(&engine, frames[i].data, frames[i].len);
        total_bytes_fed += frames[i].len;

        /* After the first match fires, inject consume() (step 3). */
        if (!consumed_first && event_count >= 1 && events[0].is_match) {
            nabtoshell_pattern_engine_consume(&engine);
            consumed_first = true;
            consume_position = total_bytes_fed;
            fprintf(stderr, "[debug] consume at raw=%zu stripped=%zu\n",
                    total_bytes_fed, engine.buffer.total_appended);
        }
    }

    fprintf(stderr, "[debug] end of replay: raw=%zu stripped=%zu\n",
            total_bytes_fed, engine.buffer.total_appended);
    print_timeline();
    ck_assert(consumed_first);

    /* Count match events after consume. The second prompt (step 4-5) must
     * produce at least one match. Without consume(), the engine only sees
     * the first match and misses the second prompt entirely. */
    int post_consume_matches = 0;
    for (int i = 0; i < event_count; i++) {
        if (events[i].is_match && events[i].bytes_fed > consume_position) {
            post_consume_matches++;
        }
    }

    ck_assert_msg(post_consume_matches >= 1,
                  "Expected at least 1 match after consume (the second prompt), "
                  "got %d. The second prompt was missed.", post_consume_matches);

    free_frames(frames, frame_count);
    nabtoshell_pattern_engine_free(&engine);
    nabtoshell_pattern_config_free(config);
}
END_TEST

Suite *pty_replay_suite(void)
{
    Suite *s = suite_create("PTY Replay");
    TCase *tc = tcase_create("Replay");
    tcase_set_timeout(tc, 60);
    tcase_add_test(tc, test_pty_replay_baseline);
    tcase_add_test(tc, test_consume_suppresses_tui_redraws);
    tcase_add_test(tc, test_second_prompt_missed_after_consume);
    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = pty_replay_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed == 0 ? 0 : 1;
}
