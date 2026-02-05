/*
 * test_buffer.c - Tests for gap buffer implementation
 *
 * Tests the critical path: buffer.c (data structure heart)
 * 
 * Copyright (C) 2026 tedit-cosmo Contributors
 * License: ISC
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "unity/unity.h"
#include "../include/buffer.h"

/* Test fixture buffer */
static Buffer *buf = NULL;

void setUp(void)
{
    buf = buffer_create(64);
}

void tearDown(void)
{
    if (buf) {
        buffer_destroy(buf);
        buf = NULL;
    }
}

/* ============================================================================
 * Creation and Destruction Tests
 * ============================================================================ */

void test_buffer_create_not_null(void)
{
    TEST_ASSERT_NOT_NULL(buf);
}

void test_buffer_create_empty(void)
{
    TEST_ASSERT_EQUAL_UINT(0, buffer_length(buf));
}

void test_buffer_create_with_capacity(void)
{
    Buffer *b = buffer_create(1024);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_UINT(0, buffer_length(b));
    buffer_destroy(b);
}

void test_buffer_create_small_capacity(void)
{
    /* Small capacity should be expanded to GAP_SIZE */
    Buffer *b = buffer_create(16);
    TEST_ASSERT_NOT_NULL(b);
    buffer_destroy(b);
}

void test_buffer_destroy_null_safe(void)
{
    /* Should not crash on NULL */
    buffer_destroy(NULL);
    TEST_PASS();
}

/* ============================================================================
 * Insert Tests
 * ============================================================================ */

void test_buffer_insert_single_char(void)
{
    buffer_insert(buf, 0, "A", 1);
    TEST_ASSERT_EQUAL_UINT(1, buffer_length(buf));
    TEST_ASSERT_EQUAL_CHAR('A', buffer_char_at(buf, 0));
}

void test_buffer_insert_string(void)
{
    const char *str = "Hello";
    buffer_insert(buf, 0, str, 5);
    TEST_ASSERT_EQUAL_UINT(5, buffer_length(buf));
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello", out);
}

void test_buffer_insert_at_beginning(void)
{
    buffer_insert(buf, 0, "World", 5);
    buffer_insert(buf, 0, "Hello ", 6);
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello World", out);
}

void test_buffer_insert_at_middle(void)
{
    buffer_insert(buf, 0, "HWorld", 6);
    buffer_insert(buf, 1, "ello ", 5);
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello World", out);
}

void test_buffer_insert_at_end(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    buffer_insert(buf, 5, " World", 6);
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello World", out);
}

void test_buffer_insert_beyond_length(void)
{
    /* Inserting beyond length should clamp to end */
    buffer_insert(buf, 0, "Hello", 5);
    buffer_insert(buf, 100, " World", 6);  /* pos > length, should append */
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello World", out);
}

void test_buffer_insert_empty_string(void)
{
    buffer_insert(buf, 0, "Test", 4);
    buffer_insert(buf, 2, "", 0);  /* Insert nothing */
    
    TEST_ASSERT_EQUAL_UINT(4, buffer_length(buf));
}

void test_buffer_insert_triggers_grow(void)
{
    /* Insert more than initial capacity */
    const char *big = "This is a much longer string that should trigger buffer growth when inserted multiple times. ";
    
    for (int i = 0; i < 20; i++) {
        buffer_insert(buf, buffer_length(buf), big, strlen(big));
    }
    
    TEST_ASSERT_EQUAL_UINT(strlen(big) * 20, buffer_length(buf));
}

/* ============================================================================
 * Delete Tests
 * ============================================================================ */

void test_buffer_delete_single_char(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    buffer_delete(buf, 0, 1);
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("ello", out);
}

void test_buffer_delete_middle(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    buffer_delete(buf, 2, 2);  /* Delete "ll" */
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Heo", out);
}

void test_buffer_delete_at_end(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    buffer_delete(buf, 3, 2);  /* Delete "lo" */
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hel", out);
}

void test_buffer_delete_all(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    buffer_delete(buf, 0, 5);
    
    TEST_ASSERT_EQUAL_UINT(0, buffer_length(buf));
}

void test_buffer_delete_beyond_length(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    buffer_delete(buf, 3, 100);  /* Delete more than available */
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hel", out);
}

void test_buffer_delete_at_invalid_pos(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    buffer_delete(buf, 100, 1);  /* pos > length, should do nothing */
    
    TEST_ASSERT_EQUAL_UINT(5, buffer_length(buf));
}

void test_buffer_delete_zero_length(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    buffer_delete(buf, 2, 0);  /* Delete nothing */
    
    TEST_ASSERT_EQUAL_UINT(5, buffer_length(buf));
}

/* ============================================================================
 * char_at Tests
 * ============================================================================ */

void test_buffer_char_at_beginning(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    TEST_ASSERT_EQUAL_CHAR('H', buffer_char_at(buf, 0));
}

void test_buffer_char_at_middle(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    TEST_ASSERT_EQUAL_CHAR('l', buffer_char_at(buf, 2));
}

void test_buffer_char_at_end(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    TEST_ASSERT_EQUAL_CHAR('o', buffer_char_at(buf, 4));
}

void test_buffer_char_at_beyond_length(void)
{
    buffer_insert(buf, 0, "Hello", 5);
    TEST_ASSERT_EQUAL_CHAR('\0', buffer_char_at(buf, 100));
}

void test_buffer_char_at_empty(void)
{
    TEST_ASSERT_EQUAL_CHAR('\0', buffer_char_at(buf, 0));
}

/* ============================================================================
 * get_text Tests
 * ============================================================================ */

void test_buffer_get_text_full(void)
{
    buffer_insert(buf, 0, "Hello World", 11);
    
    char out[64];
    size_t len = buffer_get_text(buf, out, sizeof(out));
    
    TEST_ASSERT_EQUAL_UINT(11, len);
    TEST_ASSERT_EQUAL_STRING("Hello World", out);
}

void test_buffer_get_text_truncated(void)
{
    buffer_insert(buf, 0, "Hello World", 11);
    
    char out[6];  /* Only room for "Hello" + null */
    size_t len = buffer_get_text(buf, out, sizeof(out));
    
    TEST_ASSERT_EQUAL_UINT(5, len);
    TEST_ASSERT_EQUAL_STRING("Hello", out);
}

void test_buffer_get_text_empty(void)
{
    char out[64];
    size_t len = buffer_get_text(buf, out, sizeof(out));
    
    TEST_ASSERT_EQUAL_UINT(0, len);
    TEST_ASSERT_EQUAL_STRING("", out);
}

/* ============================================================================
 * clear Tests
 * ============================================================================ */

void test_buffer_clear(void)
{
    buffer_insert(buf, 0, "Hello World", 11);
    buffer_clear(buf);
    
    TEST_ASSERT_EQUAL_UINT(0, buffer_length(buf));
}

void test_buffer_clear_then_insert(void)
{
    buffer_insert(buf, 0, "Hello World", 11);
    buffer_clear(buf);
    buffer_insert(buf, 0, "New", 3);
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("New", out);
}

/* ============================================================================
 * Gap Movement Tests (implicitly through insert/delete patterns)
 * ============================================================================ */

void test_buffer_gap_move_forward(void)
{
    /* Insert at beginning, then at end (gap moves forward) */
    buffer_insert(buf, 0, "Hello", 5);
    buffer_insert(buf, 5, " World", 6);
    buffer_insert(buf, 11, "!", 1);
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello World!", out);
}

void test_buffer_gap_move_backward(void)
{
    /* Insert at end, then at beginning (gap moves backward) */
    buffer_insert(buf, 0, "World", 5);
    buffer_insert(buf, 0, "Hello ", 6);
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Hello World", out);
}

void test_buffer_gap_move_random(void)
{
    /* Random insertions to stress gap movement */
    buffer_insert(buf, 0, "AC", 2);      /* "AC" */
    buffer_insert(buf, 1, "B", 1);        /* "ABC" */
    buffer_insert(buf, 0, "0", 1);        /* "0ABC" */
    buffer_insert(buf, 4, "D", 1);        /* "0ABCD" */
    buffer_insert(buf, 2, "X", 1);        /* "0AXBCD" */
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("0AXBCD", out);
}

/* ============================================================================
 * Edge Cases and Stress Tests
 * ============================================================================ */

void test_buffer_newlines(void)
{
    buffer_insert(buf, 0, "Line1\nLine2\nLine3", 17);
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("Line1\nLine2\nLine3", out);
}

void test_buffer_binary_data(void)
{
    /* Buffer should handle binary data (including nulls) */
    char data[] = {'A', 'B', '\0', 'C', 'D'};
    buffer_insert(buf, 0, data, 5);
    
    TEST_ASSERT_EQUAL_UINT(5, buffer_length(buf));
    TEST_ASSERT_EQUAL_CHAR('A', buffer_char_at(buf, 0));
    TEST_ASSERT_EQUAL_CHAR('\0', buffer_char_at(buf, 2));
    TEST_ASSERT_EQUAL_CHAR('D', buffer_char_at(buf, 4));
}

void test_buffer_large_insert_delete_cycle(void)
{
    /* Stress test: many insert/delete cycles */
    for (int i = 0; i < 1000; i++) {
        buffer_insert(buf, 0, "X", 1);
    }
    TEST_ASSERT_EQUAL_UINT(1000, buffer_length(buf));
    
    for (int i = 0; i < 1000; i++) {
        buffer_delete(buf, 0, 1);
    }
    TEST_ASSERT_EQUAL_UINT(0, buffer_length(buf));
}

void test_buffer_interleaved_operations(void)
{
    /* Interleaved insert and delete */
    buffer_insert(buf, 0, "ABCDE", 5);
    buffer_delete(buf, 1, 1);  /* ACDE */
    buffer_insert(buf, 2, "X", 1);  /* ACXDE */
    buffer_delete(buf, 0, 1);  /* CXDE */
    buffer_insert(buf, 4, "F", 1);  /* CXDEF */
    
    char out[64];
    buffer_get_text(buf, out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("CXDEF", out);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    UNITY_BEGIN();
    
    /* Creation and Destruction */
    RUN_TEST(test_buffer_create_not_null);
    RUN_TEST(test_buffer_create_empty);
    RUN_TEST(test_buffer_create_with_capacity);
    RUN_TEST(test_buffer_create_small_capacity);
    RUN_TEST(test_buffer_destroy_null_safe);
    
    /* Insert */
    RUN_TEST(test_buffer_insert_single_char);
    RUN_TEST(test_buffer_insert_string);
    RUN_TEST(test_buffer_insert_at_beginning);
    RUN_TEST(test_buffer_insert_at_middle);
    RUN_TEST(test_buffer_insert_at_end);
    RUN_TEST(test_buffer_insert_beyond_length);
    RUN_TEST(test_buffer_insert_empty_string);
    RUN_TEST(test_buffer_insert_triggers_grow);
    
    /* Delete */
    RUN_TEST(test_buffer_delete_single_char);
    RUN_TEST(test_buffer_delete_middle);
    RUN_TEST(test_buffer_delete_at_end);
    RUN_TEST(test_buffer_delete_all);
    RUN_TEST(test_buffer_delete_beyond_length);
    RUN_TEST(test_buffer_delete_at_invalid_pos);
    RUN_TEST(test_buffer_delete_zero_length);
    
    /* char_at */
    RUN_TEST(test_buffer_char_at_beginning);
    RUN_TEST(test_buffer_char_at_middle);
    RUN_TEST(test_buffer_char_at_end);
    RUN_TEST(test_buffer_char_at_beyond_length);
    RUN_TEST(test_buffer_char_at_empty);
    
    /* get_text */
    RUN_TEST(test_buffer_get_text_full);
    RUN_TEST(test_buffer_get_text_truncated);
    RUN_TEST(test_buffer_get_text_empty);
    
    /* clear */
    RUN_TEST(test_buffer_clear);
    RUN_TEST(test_buffer_clear_then_insert);
    
    /* Gap Movement */
    RUN_TEST(test_buffer_gap_move_forward);
    RUN_TEST(test_buffer_gap_move_backward);
    RUN_TEST(test_buffer_gap_move_random);
    
    /* Edge Cases */
    RUN_TEST(test_buffer_newlines);
    RUN_TEST(test_buffer_binary_data);
    RUN_TEST(test_buffer_large_insert_delete_cycle);
    RUN_TEST(test_buffer_interleaved_operations);
    
    return UNITY_END();
}
