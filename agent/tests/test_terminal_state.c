#include <check.h>
#include <string.h>

#include "nabtoshell_terminal_state.h"

START_TEST(test_basic_render)
{
    nabtoshell_terminal_state state;
    nabtoshell_terminal_state_init(&state, 5, 20);

    const char* text = "hello\nworld";
    nabtoshell_terminal_state_feed(&state, (const uint8_t*)text, strlen(text));

    nabtoshell_terminal_snapshot snap;
    ck_assert(nabtoshell_terminal_state_snapshot(&state, &snap));

    ck_assert_str_eq(snap.lines[0], "hello");
    ck_assert_str_eq(snap.lines[1], "world");

    nabtoshell_terminal_snapshot_free(&snap);
    nabtoshell_terminal_state_free(&state);
}
END_TEST

START_TEST(test_cursor_positioning)
{
    nabtoshell_terminal_state state;
    nabtoshell_terminal_state_init(&state, 6, 20);

    const char* text = "\x1b[4;2HChoose\x1b[5;2H1. Yes\x1b[6;2H2. No";
    nabtoshell_terminal_state_feed(&state, (const uint8_t*)text, strlen(text));

    nabtoshell_terminal_snapshot snap;
    ck_assert(nabtoshell_terminal_state_snapshot(&state, &snap));

    ck_assert_str_eq(snap.lines[3], " Choose");
    ck_assert_str_eq(snap.lines[4], " 1. Yes");
    ck_assert_str_eq(snap.lines[5], " 2. No");

    nabtoshell_terminal_snapshot_free(&snap);
    nabtoshell_terminal_state_free(&state);
}
END_TEST

START_TEST(test_chunked_csi_sequence)
{
    nabtoshell_terminal_state state;
    nabtoshell_terminal_state_init(&state, 4, 20);

    const uint8_t part1[] = {0x1b, '[', '2', ';'};
    const uint8_t part2[] = {'1', 'H', 'O', 'K'};

    nabtoshell_terminal_state_feed(&state, part1, sizeof(part1));
    nabtoshell_terminal_state_feed(&state, part2, sizeof(part2));

    nabtoshell_terminal_snapshot snap;
    ck_assert(nabtoshell_terminal_state_snapshot(&state, &snap));

    ck_assert_str_eq(snap.lines[1], "OK");

    nabtoshell_terminal_snapshot_free(&snap);
    nabtoshell_terminal_state_free(&state);
}
END_TEST

Suite* terminal_state_suite(void)
{
    Suite* s = suite_create("TerminalState");
    TCase* tc = tcase_create("Core");

    tcase_add_test(tc, test_basic_render);
    tcase_add_test(tc, test_cursor_positioning);
    tcase_add_test(tc, test_chunked_csi_sequence);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite* s = terminal_state_suite();
    SRunner* sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed == 0 ? 0 : 1;
}
