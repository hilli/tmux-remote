#include <check.h>
#include <string.h>
#include <stdlib.h>
#include "nabtoshell_ansi_stripper.h"

static nabtoshell_ansi_stripper stripper;

static void setup(void)
{
    nabtoshell_ansi_stripper_init(&stripper);
}

// Helper: feed bytes and return malloc'd string result
static char *feed_str(const uint8_t *in, size_t in_len)
{
    uint8_t out[4096];
    size_t out_len = nabtoshell_ansi_stripper_feed(&stripper, in, in_len, out, sizeof(out));
    char *result = malloc(out_len + 1);
    memcpy(result, out, out_len);
    result[out_len] = '\0';
    return result;
}

static char *feed_string(const char *s)
{
    return feed_str((const uint8_t *)s, strlen(s));
}

START_TEST(test_plain_text_passthrough)
{
    char *r = feed_string("Hello, world!");
    ck_assert_str_eq(r, "Hello, world!");
    free(r);
}
END_TEST

START_TEST(test_newlines)
{
    char *r = feed_string("line1\nline2\r\nline3");
    ck_assert_str_eq(r, "line1\nline2\n\nline3");
    free(r);
}
END_TEST

START_TEST(test_tab_expansion)
{
    char *r = feed_string("a\tb");
    ck_assert_str_eq(r, "a    b");
    free(r);
}
END_TEST

START_TEST(test_csi_stripping)
{
    // ESC[31m (red) Hello ESC[0m (reset)
    uint8_t bytes[] = {0x1B, 0x5B, 0x33, 0x31, 0x6D,
                       'H','e','l','l','o',
                       0x1B, 0x5B, 0x30, 0x6D};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "Hello");
    free(r);
}
END_TEST

START_TEST(test_csi_cursor_position_emits_newline)
{
    // ESC[10;20H followed by text
    uint8_t bytes[] = {0x1B, 0x5B, 0x31, 0x30, 0x3B, 0x32, 0x30, 0x48,
                       't','e','x','t'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "\ntext");
    free(r);
}
END_TEST

START_TEST(test_osc_stripping_with_bel)
{
    // ESC ] 0 ; title BEL visible
    uint8_t bytes[] = {0x1B, 0x5D, 0x30, 0x3B,
                       'w','i','n','d','o','w',' ','t','i','t','l','e',
                       0x07,
                       'v','i','s','i','b','l','e'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "visible");
    free(r);
}
END_TEST

START_TEST(test_osc_stripping_with_st)
{
    // ESC ] 0 ; title ESC \ visible
    uint8_t bytes[] = {0x1B, 0x5D, 0x30, 0x3B,
                       't','i','t','l','e',
                       0x1B, 0x5C,
                       'v','i','s','i','b','l','e'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "visible");
    free(r);
}
END_TEST

START_TEST(test_cross_chunk_csi)
{
    char *r1 = feed_str((uint8_t[]){0x1B}, 1);
    ck_assert_str_eq(r1, "");
    free(r1);

    char *r2 = feed_str((uint8_t[]){0x5B, 0x33, 0x31, 0x6D}, 4);
    ck_assert_str_eq(r2, "");
    free(r2);

    char *r3 = feed_string("Hello");
    ck_assert_str_eq(r3, "Hello");
    free(r3);
}
END_TEST

START_TEST(test_cross_chunk_osc)
{
    char *r1 = feed_str((uint8_t[]){0x1B, 0x5D, 0x30, 0x3B}, 4);
    ck_assert_str_eq(r1, "");
    free(r1);

    char *r2 = feed_string("title");
    ck_assert_str_eq(r2, "");
    free(r2);

    uint8_t bytes3[] = {0x07, 'v','i','s','i','b','l','e'};
    char *r3 = feed_str(bytes3, sizeof(bytes3));
    ck_assert_str_eq(r3, "visible");
    free(r3);
}
END_TEST

START_TEST(test_two_byte_escape)
{
    // ESC M (reverse index) should be stripped
    uint8_t bytes[] = {0x1B, 0x4D, 't','e','x','t'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "text");
    free(r);
}
END_TEST

START_TEST(test_c0_controls_stripped)
{
    // BEL (0x07) and BS (0x08) should be stripped
    uint8_t bytes[] = {'a','b','c', 0x07, 0x08, 'd','e','f'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "abcdef");
    free(r);
}
END_TEST

START_TEST(test_mixed_content)
{
    // ESC[1m bold ESC[0m normal ESC]0;titleBEL more
    uint8_t bytes[64];
    int n = 0;
    bytes[n++] = 0x1B; bytes[n++] = 0x5B; bytes[n++] = 0x31; bytes[n++] = 0x6D;
    memcpy(bytes+n, "bold ", 5); n += 5;
    bytes[n++] = 0x1B; bytes[n++] = 0x5B; bytes[n++] = 0x30; bytes[n++] = 0x6D;
    memcpy(bytes+n, "normal ", 7); n += 7;
    bytes[n++] = 0x1B; bytes[n++] = 0x5D; bytes[n++] = 0x30; bytes[n++] = 0x3B;
    memcpy(bytes+n, "title", 5); n += 5;
    bytes[n++] = 0x07;
    memcpy(bytes+n, "more", 4); n += 4;

    char *r = feed_str(bytes, n);
    ck_assert_str_eq(r, "bold normal more");
    free(r);
}
END_TEST

START_TEST(test_csi_cursor_forward_emits_space)
{
    // hello ESC[5C world
    uint8_t bytes[] = {'h','e','l','l','o',
                       0x1B, 0x5B, 0x35, 0x43,
                       'w','o','r','l','d'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "hello world");
    free(r);
}
END_TEST

START_TEST(test_csi_cursor_down_emits_newline)
{
    // line1 ESC[1B line2
    uint8_t bytes[] = {'l','i','n','e','1',
                       0x1B, 0x5B, 0x31, 0x42,
                       'l','i','n','e','2'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "line1\nline2");
    free(r);
}
END_TEST

START_TEST(test_csi_cursor_horizontal_absolute_emits_space)
{
    // hello ESC[10G world
    uint8_t bytes[] = {'h','e','l','l','o',
                       0x1B, 0x5B, 0x31, 0x30, 0x47,
                       'w','o','r','l','d'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "hello world");
    free(r);
}
END_TEST

START_TEST(test_csi_sgr_no_whitespace)
{
    // ESC[36m colored ESC[0m text
    uint8_t bytes[] = {0x1B, 0x5B, 0x33, 0x36, 0x6D,
                       'c','o','l','o','r','e','d',
                       0x1B, 0x5B, 0x30, 0x6D,
                       ' ','t','e','x','t'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "colored text");
    free(r);
}
END_TEST

START_TEST(test_charset_designation_stripped)
{
    // ESC(B should not leak 'B'
    uint8_t bytes[] = {'h','e','l','l','o',
                       0x1B, 0x28, 0x42,
                       ' ','w','o','r','l','d'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "hello world");
    free(r);
}
END_TEST

START_TEST(test_charset_designation_g1)
{
    // ESC)0
    uint8_t bytes[] = {'b','e','f','o','r','e',
                       0x1B, 0x29, 0x30,
                       'a','f','t','e','r'};
    char *r = feed_str(bytes, sizeof(bytes));
    ck_assert_str_eq(r, "beforeafter");
    free(r);
}
END_TEST

START_TEST(test_ink_style_cursor_positioning)
{
    uint8_t bytes[128];
    int n = 0;
    // ESC[1;1H
    bytes[n++] = 0x1B; bytes[n++] = 0x5B; bytes[n++] = 0x31; bytes[n++] = 0x3B; bytes[n++] = 0x31; bytes[n++] = 0x48;
    memcpy(bytes+n, "Do you want", 11); n += 11;
    // ESC[1C
    bytes[n++] = 0x1B; bytes[n++] = 0x5B; bytes[n++] = 0x31; bytes[n++] = 0x43;
    memcpy(bytes+n, "to proceed?", 11); n += 11;
    // ESC[2;1H
    bytes[n++] = 0x1B; bytes[n++] = 0x5B; bytes[n++] = 0x32; bytes[n++] = 0x3B; bytes[n++] = 0x31; bytes[n++] = 0x48;
    memcpy(bytes+n, "1. Yes", 6); n += 6;
    // ESC[3;1H
    bytes[n++] = 0x1B; bytes[n++] = 0x5B; bytes[n++] = 0x33; bytes[n++] = 0x3B; bytes[n++] = 0x31; bytes[n++] = 0x48;
    memcpy(bytes+n, "2. No", 5); n += 5;

    char *r = feed_str(bytes, n);
    ck_assert_str_eq(r, "\nDo you want to proceed?\n1. Yes\n2. No");
    free(r);
}
END_TEST

START_TEST(test_reset_clears_state)
{
    feed_str((uint8_t[]){0x1B}, 1);
    nabtoshell_ansi_stripper_reset(&stripper);
    char *r = feed_string("hello");
    ck_assert_str_eq(r, "hello");
    free(r);
}
END_TEST

START_TEST(test_empty_input)
{
    char *r = feed_str((uint8_t[]){0}, 0);
    ck_assert_str_eq(r, "");
    free(r);
}
END_TEST

// UTF-8 cross-chunk tests

START_TEST(test_utf8_two_bytes_split)
{
    // U+00B7 MIDDLE DOT: c2 b7
    uint8_t chunk1[] = {'h','e','l','l','o', 0xC2};
    char *r1 = feed_str(chunk1, sizeof(chunk1));
    ck_assert_str_eq(r1, "hello");
    free(r1);

    uint8_t chunk2[] = {0xB7, ' ','w','o','r','l','d'};
    char *r2 = feed_str(chunk2, sizeof(chunk2));
    ck_assert_str_eq(r2, "\xC2\xB7 world");
    free(r2);
}
END_TEST

START_TEST(test_utf8_three_bytes_split_after_first)
{
    // U+276F: e2 9d af
    uint8_t chunk1[] = {'b','e','f','o','r','e', 0xE2};
    char *r1 = feed_str(chunk1, sizeof(chunk1));
    ck_assert_str_eq(r1, "before");
    free(r1);

    uint8_t chunk2[] = {0x9D, 0xAF, ' ','a','f','t','e','r'};
    char *r2 = feed_str(chunk2, sizeof(chunk2));
    ck_assert_str_eq(r2, "\xE2\x9D\xAF after");
    free(r2);
}
END_TEST

START_TEST(test_utf8_three_bytes_split_after_second)
{
    // U+276F: e2 9d af -- split after second byte
    uint8_t chunk1[] = {'x', 0xE2, 0x9D};
    char *r1 = feed_str(chunk1, sizeof(chunk1));
    ck_assert_str_eq(r1, "x");
    free(r1);

    uint8_t chunk2[] = {0xAF};
    char *r2 = feed_str(chunk2, sizeof(chunk2));
    ck_assert_str_eq(r2, "\xE2\x9D\xAF");
    free(r2);
}
END_TEST

START_TEST(test_utf8_bulk_split)
{
    // Many c2 b7 pairs, split mid-sequence
    uint8_t dots[108];
    for (int i = 0; i < 54; i++) {
        dots[i*2] = 0xC2;
        dots[i*2+1] = 0xB7;
    }
    // Split so first chunk ends with c2
    uint8_t out1[256], out2[256];
    size_t len1 = nabtoshell_ansi_stripper_feed(&stripper, dots, 53, out1, sizeof(out1));
    size_t len2 = nabtoshell_ansi_stripper_feed(&stripper, dots + 53, 108 - 53, out2, sizeof(out2));

    // Build expected: 54 middle dots (each 2 bytes in UTF-8)
    uint8_t expected[108];
    for (int i = 0; i < 54; i++) {
        expected[i*2] = 0xC2;
        expected[i*2+1] = 0xB7;
    }

    ck_assert_int_eq(len1 + len2, 108);
    uint8_t combined[256];
    memcpy(combined, out1, len1);
    memcpy(combined + len1, out2, len2);
    ck_assert_int_eq(memcmp(combined, expected, 108), 0);
}
END_TEST

START_TEST(test_utf8_complete_not_held_back)
{
    uint8_t chunk[] = {'a', 0xC2, 0xB7};
    char *r = feed_str(chunk, sizeof(chunk));
    ck_assert_str_eq(r, "a\xC2\xB7");
    free(r);
}
END_TEST

START_TEST(test_utf8_reset_clears_pending)
{
    feed_str((uint8_t[]){0xE2}, 1);
    nabtoshell_ansi_stripper_reset(&stripper);
    char *r = feed_string("clean");
    ck_assert_str_eq(r, "clean");
    free(r);
}
END_TEST

Suite *ansi_stripper_suite(void)
{
    Suite *s = suite_create("ANSIStripper");
    TCase *tc = tcase_create("Core");
    tcase_add_checked_fixture(tc, setup, NULL);

    tcase_add_test(tc, test_plain_text_passthrough);
    tcase_add_test(tc, test_newlines);
    tcase_add_test(tc, test_tab_expansion);
    tcase_add_test(tc, test_csi_stripping);
    tcase_add_test(tc, test_csi_cursor_position_emits_newline);
    tcase_add_test(tc, test_osc_stripping_with_bel);
    tcase_add_test(tc, test_osc_stripping_with_st);
    tcase_add_test(tc, test_cross_chunk_csi);
    tcase_add_test(tc, test_cross_chunk_osc);
    tcase_add_test(tc, test_two_byte_escape);
    tcase_add_test(tc, test_c0_controls_stripped);
    tcase_add_test(tc, test_mixed_content);
    tcase_add_test(tc, test_csi_cursor_forward_emits_space);
    tcase_add_test(tc, test_csi_cursor_down_emits_newline);
    tcase_add_test(tc, test_csi_cursor_horizontal_absolute_emits_space);
    tcase_add_test(tc, test_csi_sgr_no_whitespace);
    tcase_add_test(tc, test_charset_designation_stripped);
    tcase_add_test(tc, test_charset_designation_g1);
    tcase_add_test(tc, test_ink_style_cursor_positioning);
    tcase_add_test(tc, test_reset_clears_state);
    tcase_add_test(tc, test_empty_input);
    tcase_add_test(tc, test_utf8_two_bytes_split);
    tcase_add_test(tc, test_utf8_three_bytes_split_after_first);
    tcase_add_test(tc, test_utf8_three_bytes_split_after_second);
    tcase_add_test(tc, test_utf8_bulk_split);
    tcase_add_test(tc, test_utf8_complete_not_held_back);
    tcase_add_test(tc, test_utf8_reset_clears_pending);

    suite_add_tcase(s, tc);
    return s;
}

int main(void)
{
    Suite *s = ansi_stripper_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return failed == 0 ? 0 : 1;
}
