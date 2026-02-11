#include <check.h>
#include <string.h>
#include <stdlib.h>
#include "nabtoshell_pattern_matcher.h"
#include "nabtoshell_pattern_config.h"

static nabtoshell_pattern_matcher matcher;

static void setup(void)
{
    nabtoshell_pattern_matcher_init(&matcher);
}

static void teardown(void)
{
    nabtoshell_pattern_matcher_reset(&matcher);
}

// Helper to create a yes/no pattern definition
static nabtoshell_pattern_definition make_yn_def(const char *id, const char *regex,
                                                   const char *label1, const char *keys1,
                                                   const char *label2, const char *keys2)
{
    nabtoshell_pattern_definition def;
    memset(&def, 0, sizeof(def));
    def.id = strdup(id);
    def.type = PATTERN_TYPE_YES_NO;
    def.regex = strdup(regex);
    def.multi_line = false;
    def.actions = calloc(2, sizeof(nabtoshell_pattern_action));
    def.actions[0].label = strdup(label1);
    def.actions[0].keys = strdup(keys1);
    def.actions[1].label = strdup(label2);
    def.actions[1].keys = strdup(keys2);
    def.action_count = 2;
    def.action_template = NULL;
    return def;
}

static nabtoshell_pattern_definition make_menu_def(const char *id, const char *regex, const char *tmpl_keys)
{
    nabtoshell_pattern_definition def;
    memset(&def, 0, sizeof(def));
    def.id = strdup(id);
    def.type = PATTERN_TYPE_NUMBERED_MENU;
    def.regex = strdup(regex);
    def.multi_line = true;
    def.actions = NULL;
    def.action_count = 0;
    def.action_template = calloc(1, sizeof(nabtoshell_pattern_action_template));
    def.action_template->keys = strdup(tmpl_keys);
    return def;
}

static void free_def(nabtoshell_pattern_definition *d)
{
    free(d->id);
    free(d->regex);
    for (int i = 0; i < d->action_count; i++) {
        free(d->actions[i].label);
        free(d->actions[i].keys);
    }
    free(d->actions);
    if (d->action_template) {
        free(d->action_template->keys);
        free(d->action_template);
    }
}

START_TEST(test_yes_no_match)
{
    nabtoshell_pattern_definition def = make_yn_def("yn", "Continue\\? \\(y/n\\)", "Yes", "y", "No", "n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "Some output\nContinue? (y/n)";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 100);
    ck_assert_ptr_nonnull(m);
    ck_assert_str_eq(m->id, "yn");
    ck_assert_int_eq(m->pattern_type, PATTERN_TYPE_YES_NO);
    ck_assert_int_eq(m->action_count, 2);
    ck_assert_str_eq(m->actions[0].label, "Yes");
    ck_assert_str_eq(m->actions[0].keys, "y");
    ck_assert_str_eq(m->actions[1].label, "No");
    ck_assert_str_eq(m->actions[1].keys, "n");
    ck_assert_ptr_null(m->prompt);

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

START_TEST(test_no_match)
{
    nabtoshell_pattern_definition def = make_yn_def("yn", "Continue\\? \\(y/n\\)", "Yes", "y", "No", "n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "Some other output without a prompt";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 100);
    ck_assert_ptr_null(m);

    free_def(&def);
}
END_TEST

START_TEST(test_numbered_menu_dot_format)
{
    nabtoshell_pattern_definition def = make_menu_def("menu",
        "Do you want to proceed\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text =
        "Do you want to proceed?\n"
        "1. Yes\n"
        "2. Yes, and don't ask again for /Users/ug/git/qr-te\n"
        "3. No";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 200);

    ck_assert_ptr_nonnull(m);
    ck_assert_str_eq(m->id, "menu");
    ck_assert_int_eq(m->pattern_type, PATTERN_TYPE_NUMBERED_MENU);
    ck_assert_int_eq(m->action_count, 3);
    ck_assert_str_eq(m->actions[0].label, "Yes");
    ck_assert_str_eq(m->actions[0].keys, "1\n");
    ck_assert_str_eq(m->actions[1].label, "Yes, and don't ask again for /Users/ug/git/qr-te");
    ck_assert_str_eq(m->actions[1].keys, "2\n");
    ck_assert_str_eq(m->actions[2].label, "No");
    ck_assert_str_eq(m->actions[2].keys, "3\n");

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

START_TEST(test_numbered_menu_extracts_prompt)
{
    nabtoshell_pattern_definition def = make_menu_def("menu",
        "Do you want to proceed\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "Do you want to proceed?\n1. Yes\n2. No";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 200);

    ck_assert_ptr_nonnull(m);
    ck_assert_ptr_nonnull(m->prompt);
    ck_assert_str_eq(m->prompt, "Do you want to proceed?");

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

START_TEST(test_tmux_status_bar_no_false_positive)
{
    nabtoshell_pattern_definition def = make_menu_def("numbered_prompt",
        "Do you want to (?:proceed|continue)\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "[1] 0:node* 1:bash  \"hostname\" 10:30 01-Jan-25";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 100);
    ck_assert_ptr_null(m);

    free_def(&def);
}
END_TEST

START_TEST(test_priority_ordering)
{
    nabtoshell_pattern_definition defs[2];
    defs[0] = make_yn_def("first", "prompt", "A", "a", "B", "b");
    defs[1] = make_yn_def("second", "prompt", "C", "c", "D", "d");
    defs[1].type = PATTERN_TYPE_ACCEPT_REJECT;
    nabtoshell_pattern_matcher_load(&matcher, defs, 2);

    const char *text = "some prompt here";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 50);
    ck_assert_ptr_nonnull(m);
    ck_assert_str_eq(m->id, "first");

    nabtoshell_pattern_match_free(m);
    free_def(&defs[0]);
    free_def(&defs[1]);
}
END_TEST

START_TEST(test_accept_reject_match)
{
    nabtoshell_pattern_definition def;
    memset(&def, 0, sizeof(def));
    def.id = strdup("diff");
    def.type = PATTERN_TYPE_ACCEPT_REJECT;
    def.regex = strdup("Apply these changes\\?");
    def.actions = calloc(2, sizeof(nabtoshell_pattern_action));
    def.actions[0].label = strdup("Accept");
    def.actions[0].keys = strdup("y");
    def.actions[1].label = strdup("Reject");
    def.actions[1].keys = strdup("n");
    def.action_count = 2;
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "Apply these changes?";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 100);
    ck_assert_ptr_nonnull(m);
    ck_assert_int_eq(m->pattern_type, PATTERN_TYPE_ACCEPT_REJECT);
    ck_assert_int_eq(m->action_count, 2);

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

START_TEST(test_invalid_regex_skipped)
{
    nabtoshell_pattern_definition defs[2];
    defs[0] = make_yn_def("bad", "[invalid", "Y", "y", "N", "n");
    defs[1] = make_yn_def("good", "hello", "Y", "y", "N", "n");
    nabtoshell_pattern_matcher_load(&matcher, defs, 2);

    const char *text = "hello";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 10);
    ck_assert_ptr_nonnull(m);
    ck_assert_str_eq(m->id, "good");

    nabtoshell_pattern_match_free(m);
    free_def(&defs[0]);
    free_def(&defs[1]);
}
END_TEST

START_TEST(test_empty_patterns_no_match)
{
    nabtoshell_pattern_matcher_load(&matcher, NULL, 0);
    const char *text = "anything";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 10);
    ck_assert_ptr_null(m);
}
END_TEST

START_TEST(test_reset_clears_patterns)
{
    nabtoshell_pattern_definition def = make_yn_def("p", "test", "Y", "y", "N", "n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);
    nabtoshell_pattern_matcher_reset(&matcher);

    const char *text = "test";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 10);
    ck_assert_ptr_null(m);

    free_def(&def);
}
END_TEST

START_TEST(test_yes_no_has_nil_prompt)
{
    nabtoshell_pattern_definition def = make_yn_def("yn", "Allow\\? \\(y/n\\)", "Allow", "y", "Deny", "n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "Allow? (y/n)";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 100);
    ck_assert_ptr_nonnull(m);
    ck_assert_ptr_null(m->prompt);

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

START_TEST(test_claude_code_real_prompt_with_arrow)
{
    nabtoshell_pattern_definition def = make_menu_def("numbered_prompt",
        "Do you want to (?:proceed|continue)\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text =
        "Do you want to proceed?\n"
        "> 1. Yes\n"
        "  2. Yes, and don't ask again for /Users/ug/git/qr-te\n"
        "  3. No\n"
        "\n"
        "Esc to cancel";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 300);

    ck_assert_ptr_nonnull(m);
    ck_assert_ptr_nonnull(m->prompt);
    ck_assert_str_eq(m->prompt, "Do you want to proceed?");
    ck_assert_int_eq(m->action_count, 3);
    ck_assert_str_eq(m->actions[0].label, "Yes");
    ck_assert_str_eq(m->actions[2].label, "No");

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

START_TEST(test_claude_code_real_prompt_with_heavy_angle)
{
    nabtoshell_pattern_definition def = make_menu_def("numbered_prompt",
        "Do you want to (?:proceed|continue)\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    // U+276F = \xe2\x9d\xaf, U+00B7 = \xc2\xb7
    const char *text =
        " Do you want to proceed?\n"
        "\n"
        " \xE2\x9D\xAF 1. Yes\n"
        " 2. Yes, and don't ask again\n"
        " 3. No\n"
        "\n"
        "\n"
        "\n"
        " Esc to cancel \xC2\xB7 Tab to amend \xC2\xB7 ctrl+e to explain\n";

    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 500);

    ck_assert_ptr_nonnull(m);
    ck_assert_ptr_nonnull(m->prompt);
    ck_assert_str_eq(m->prompt, "Do you want to proceed?");
    ck_assert_int_eq(m->action_count, 3);
    ck_assert_str_eq(m->actions[0].label, "Yes");
    ck_assert_str_eq(m->actions[2].label, "No");

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

START_TEST(test_numbered_menu_ignores_earlier_items)
{
    nabtoshell_pattern_definition def = make_menu_def("numbered_prompt",
        "Do you want to (?:proceed|continue)\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text =
        "Previous output with 1. Yes and 2. No options\n"
        "Some more text\n"
        "Do you want to proceed?\n"
        "1. Yes\n"
        "2. Yes, and don't ask again\n"
        "3. No";

    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 500);
    ck_assert_ptr_nonnull(m);
    ck_assert_int_eq(m->action_count, 3);
    ck_assert_str_eq(m->actions[0].label, "Yes");
    ck_assert_str_eq(m->actions[1].label, "Yes, and don't ask again");
    ck_assert_str_eq(m->actions[2].label, "No");

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

START_TEST(test_claude_code_prompt_with_newlines)
{
    nabtoshell_pattern_definition def = make_menu_def("numbered_prompt",
        "Do you want to (?:proceed|continue)\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "Do you want to proceed?\n\xE2\x9D\xAF 1. Yes\n  2. No\n  3. Cancel";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 200);
    ck_assert_ptr_nonnull(m);
    ck_assert_int_eq(m->action_count, 3);

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

START_TEST(test_claude_code_prompt_with_crlf_raw_does_not_match)
{
    nabtoshell_pattern_definition def = make_menu_def("numbered_prompt",
        "Do you want to (?:proceed|continue)\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "Do you want to proceed?\r\n\xE2\x9D\xAF 1. Yes\r\n  2. No\r\n  3. Cancel";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 200);
    ck_assert_ptr_null(m);

    free_def(&def);
}
END_TEST

START_TEST(test_claude_code_prompt_no_newlines)
{
    nabtoshell_pattern_definition def = make_menu_def("numbered_prompt",
        "Do you want to (?:proceed|continue)\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "Do you want to proceed?\xE2\x9D\xAF 1. Yes  2. No  3. Cancel";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 200);
    ck_assert_ptr_null(m);

    free_def(&def);
}
END_TEST

START_TEST(test_claude_code_prompt_with_double_newlines)
{
    nabtoshell_pattern_definition def = make_menu_def("numbered_prompt",
        "Do you want to (?:proceed|continue)\\?\\n.*1\\. .+\\n.*2\\. .+", "{number}\n");
    nabtoshell_pattern_matcher_load(&matcher, &def, 1);

    const char *text = "Do you want to proceed?\n\n\xE2\x9D\xAF 1. Yes\n\n  2. No\n\n  3. Cancel";
    nabtoshell_pattern_match *m = nabtoshell_pattern_matcher_match(&matcher, text, strlen(text), 200);
    ck_assert_ptr_nonnull(m);

    nabtoshell_pattern_match_free(m);
    free_def(&def);
}
END_TEST

Suite *pattern_matcher_suite(void)
{
    Suite *s = suite_create("PatternMatcher");
    TCase *tc = tcase_create("Core");
    tcase_add_checked_fixture(tc, setup, teardown);

    tcase_add_test(tc, test_yes_no_match);
    tcase_add_test(tc, test_no_match);
    tcase_add_test(tc, test_numbered_menu_dot_format);
    tcase_add_test(tc, test_numbered_menu_extracts_prompt);
    tcase_add_test(tc, test_tmux_status_bar_no_false_positive);
    tcase_add_test(tc, test_priority_ordering);
    tcase_add_test(tc, test_accept_reject_match);
    tcase_add_test(tc, test_invalid_regex_skipped);
    tcase_add_test(tc, test_empty_patterns_no_match);
    tcase_add_test(tc, test_reset_clears_patterns);
    tcase_add_test(tc, test_yes_no_has_nil_prompt);
    tcase_add_test(tc, test_claude_code_real_prompt_with_arrow);
    tcase_add_test(tc, test_claude_code_real_prompt_with_heavy_angle);
    tcase_add_test(tc, test_numbered_menu_ignores_earlier_items);
    tcase_add_test(tc, test_claude_code_prompt_with_newlines);
    tcase_add_test(tc, test_claude_code_prompt_with_crlf_raw_does_not_match);
    tcase_add_test(tc, test_claude_code_prompt_no_newlines);
    tcase_add_test(tc, test_claude_code_prompt_with_double_newlines);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = pattern_matcher_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed == 0 ? 0 : 1;
}
