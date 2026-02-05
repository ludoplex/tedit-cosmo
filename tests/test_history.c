/*
 * test_history.c - Tests for persistent undo/redo history
 *
 * Tests the critical path: history.c (persistent undo/redo with crash recovery)
 * 
 * Copyright (C) 2026 tedit-cosmo Contributors
 * License: ISC
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <io.h>
#define unlink _unlink
#else
#include <unistd.h>
#endif

#include "unity/unity.h"
#include "../include/history.h"

/* Test file paths */
static const char *TEST_FILE = "test_file.txt";
static char TEST_HISTORY_FILE[280];

/* Test fixture history */
static History *hist = NULL;

void setUp(void)
{
    /* Generate history path */
    history_get_path(TEST_FILE, TEST_HISTORY_FILE, sizeof(TEST_HISTORY_FILE));
    
    /* Clean up any existing test files */
    unlink(TEST_FILE);
    unlink(TEST_HISTORY_FILE);
    
    /* Create fresh history */
    hist = history_open(TEST_FILE);
}

void tearDown(void)
{
    if (hist) {
        history_close(hist);
        hist = NULL;
    }
    
    /* Clean up test files */
    unlink(TEST_FILE);
    unlink(TEST_HISTORY_FILE);
}

/* ============================================================================
 * History Path Generation Tests
 * ============================================================================ */

void test_history_get_path(void)
{
    char path[280];
    history_get_path("test.txt", path, sizeof(path));
    TEST_ASSERT_EQUAL_STRING("test.txt.tedit-history", path);
}

void test_history_get_path_long(void)
{
    char path[280];
    history_get_path("/path/to/some/file.txt", path, sizeof(path));
    TEST_ASSERT_EQUAL_STRING("/path/to/some/file.txt.tedit-history", path);
}

void test_history_get_path_buffer_too_small(void)
{
    char path[10];  /* Too small */
    history_get_path("test.txt", path, sizeof(path));
    TEST_ASSERT_EQUAL_STRING("", path);  /* Should be empty on overflow */
}

/* ============================================================================
 * Timestamp Tests
 * ============================================================================ */

void test_history_timestamp_nonzero(void)
{
    uint64_t ts = history_get_timestamp();
    TEST_ASSERT_NOT_EQUAL(0, ts);
}

void test_history_timestamp_increases(void)
{
    uint64_t ts1 = history_get_timestamp();
    /* Small busy-wait to ensure time passes */
    volatile int x = 0;
    for (int i = 0; i < 100000; i++) x++;
    (void)x;
    uint64_t ts2 = history_get_timestamp();
    
    TEST_ASSERT_TRUE(ts2 >= ts1);
}

/* ============================================================================
 * Creation and Opening Tests
 * ============================================================================ */

void test_history_open_creates_file(void)
{
    TEST_ASSERT_NOT_NULL(hist);
    
    /* Check history file was created */
    FILE *f = fopen(TEST_HISTORY_FILE, "rb");
    TEST_ASSERT_NOT_NULL(f);
    if (f) fclose(f);
}

void test_history_open_empty(void)
{
    TEST_ASSERT_EQUAL_UINT(0, history_count(hist));
}

void test_history_open_file_size(void)
{
    /* New history should have header size */
    size_t size = history_size(hist);
    TEST_ASSERT_TRUE(size >= 32);  /* HistoryHeader is 32 bytes */
}

void test_history_close_null_safe(void)
{
    /* Should not crash on NULL */
    history_close(NULL);
    TEST_PASS();
}

/* ============================================================================
 * Append Tests
 * ============================================================================ */

void test_history_append_insert(void)
{
    int result = history_append(hist, OP_INSERT, 0, "Hello", 5);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(1, history_count(hist));
}

void test_history_append_delete(void)
{
    int result = history_append(hist, OP_DELETE, 0, "Hello", 5);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(1, history_count(hist));
}

void test_history_append_multiple(void)
{
    history_append(hist, OP_INSERT, 0, "Hello", 5);
    history_append(hist, OP_INSERT, 5, " World", 6);
    history_append(hist, OP_DELETE, 0, "H", 1);
    
    TEST_ASSERT_EQUAL_UINT(3, history_count(hist));
}

void test_history_append_null_returns_error(void)
{
    int result = history_append(NULL, OP_INSERT, 0, "Test", 4);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_history_append_increases_size(void)
{
    size_t size1 = history_size(hist);
    history_append(hist, OP_INSERT, 0, "Test data", 9);
    size_t size2 = history_size(hist);
    
    TEST_ASSERT_TRUE(size2 > size1);
}

/* ============================================================================
 * Undo Tests
 * ============================================================================ */

void test_history_can_undo_empty(void)
{
    TEST_ASSERT_FALSE(history_can_undo(hist));
}

void test_history_can_undo_after_append(void)
{
    history_append(hist, OP_INSERT, 0, "Hello", 5);
    TEST_ASSERT_TRUE(history_can_undo(hist));
}

void test_history_undo_returns_op(void)
{
    history_append(hist, OP_INSERT, 10, "Test", 4);
    
    EditOp *op = history_undo(hist);
    TEST_ASSERT_NOT_NULL(op);
    TEST_ASSERT_EQUAL_INT(OP_INSERT, op->type);
    TEST_ASSERT_EQUAL_UINT(10, op->position);
    TEST_ASSERT_EQUAL_UINT(4, op->length);
    TEST_ASSERT_EQUAL_STRING("Test", op->data);
}

void test_history_undo_multiple(void)
{
    history_append(hist, OP_INSERT, 0, "A", 1);
    history_append(hist, OP_INSERT, 1, "B", 1);
    history_append(hist, OP_INSERT, 2, "C", 1);
    
    EditOp *op1 = history_undo(hist);
    TEST_ASSERT_EQUAL_STRING("C", op1->data);
    
    EditOp *op2 = history_undo(hist);
    TEST_ASSERT_EQUAL_STRING("B", op2->data);
    
    EditOp *op3 = history_undo(hist);
    TEST_ASSERT_EQUAL_STRING("A", op3->data);
}

void test_history_undo_empty_returns_null(void)
{
    EditOp *op = history_undo(hist);
    TEST_ASSERT_NULL(op);
}

void test_history_undo_exhausted(void)
{
    history_append(hist, OP_INSERT, 0, "A", 1);
    
    history_undo(hist);  /* First undo */
    
    /* Can undo should be false after undoing all ops */
    TEST_ASSERT_FALSE(history_can_undo(hist));
}

/* ============================================================================
 * Redo Tests
 * ============================================================================ */

void test_history_can_redo_empty(void)
{
    TEST_ASSERT_FALSE(history_can_redo(hist));
}

void test_history_can_redo_after_undo(void)
{
    history_append(hist, OP_INSERT, 0, "Hello", 5);
    history_undo(hist);
    
    TEST_ASSERT_TRUE(history_can_redo(hist));
}

void test_history_redo_returns_op(void)
{
    history_append(hist, OP_INSERT, 10, "Test", 4);
    history_undo(hist);
    
    EditOp *op = history_redo(hist);
    TEST_ASSERT_NOT_NULL(op);
    TEST_ASSERT_EQUAL_INT(OP_INSERT, op->type);
}

void test_history_undo_redo_cycle(void)
{
    history_append(hist, OP_INSERT, 0, "Hello", 5);
    
    /* Undo */
    EditOp *op1 = history_undo(hist);
    TEST_ASSERT_NOT_NULL(op1);
    
    /* Redo */
    EditOp *op2 = history_redo(hist);
    TEST_ASSERT_NOT_NULL(op2);
    
    /* Should be same operation */
    TEST_ASSERT_EQUAL_INT(op1->type, op2->type);
    TEST_ASSERT_EQUAL_UINT(op1->position, op2->position);
}

void test_history_append_clears_redo(void)
{
    history_append(hist, OP_INSERT, 0, "A", 1);
    history_append(hist, OP_INSERT, 1, "B", 1);
    
    history_undo(hist);  /* Undo B */
    TEST_ASSERT_TRUE(history_can_redo(hist));
    
    /* New append should clear redo chain */
    history_append(hist, OP_INSERT, 1, "C", 1);
    TEST_ASSERT_FALSE(history_can_redo(hist));
}

/* ============================================================================
 * Persistence Tests (Crash Recovery)
 * ============================================================================ */

void test_history_persistence_reload(void)
{
    /* Add operations */
    history_append(hist, OP_INSERT, 0, "Hello", 5);
    history_append(hist, OP_INSERT, 5, " World", 6);
    
    /* Close and reopen */
    history_close(hist);
    hist = history_open(TEST_FILE);
    
    TEST_ASSERT_NOT_NULL(hist);
    TEST_ASSERT_EQUAL_UINT(2, history_count(hist));
}

void test_history_persistence_data_integrity(void)
{
    /* Add operation with specific data */
    history_append(hist, OP_INSERT, 42, "TestData", 8);
    
    /* Close and reopen */
    history_close(hist);
    hist = history_open(TEST_FILE);
    
    /* Undo should return correct data */
    EditOp *op = history_undo(hist);
    TEST_ASSERT_NOT_NULL(op);
    TEST_ASSERT_EQUAL_INT(OP_INSERT, op->type);
    TEST_ASSERT_EQUAL_UINT(42, op->position);
    TEST_ASSERT_EQUAL_UINT(8, op->length);
    TEST_ASSERT_EQUAL_STRING("TestData", op->data);
}

void test_history_persistence_many_ops(void)
{
    /* Add many operations */
    for (int i = 0; i < 100; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "Op%d", i);
        history_append(hist, OP_INSERT, i, buf, strlen(buf));
    }
    
    /* Close and reopen */
    history_close(hist);
    hist = history_open(TEST_FILE);
    
    TEST_ASSERT_EQUAL_UINT(100, history_count(hist));
}

/* ============================================================================
 * Clear Tests
 * ============================================================================ */

void test_history_clear(void)
{
    history_append(hist, OP_INSERT, 0, "Hello", 5);
    history_append(hist, OP_INSERT, 5, " World", 6);
    
    int result = history_clear(hist);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(0, history_count(hist));
}

void test_history_clear_resets_undo_redo(void)
{
    history_append(hist, OP_INSERT, 0, "Test", 4);
    history_clear(hist);
    
    TEST_ASSERT_FALSE(history_can_undo(hist));
    TEST_ASSERT_FALSE(history_can_redo(hist));
}

void test_history_clear_null_safe(void)
{
    int result = history_clear(NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

/* ============================================================================
 * Count and Size Tests
 * ============================================================================ */

void test_history_count_null(void)
{
    TEST_ASSERT_EQUAL_UINT(0, history_count(NULL));
}

void test_history_size_null(void)
{
    TEST_ASSERT_EQUAL_UINT(0, history_size(NULL));
}

void test_history_count_accurate(void)
{
    history_append(hist, OP_INSERT, 0, "A", 1);
    TEST_ASSERT_EQUAL_UINT(1, history_count(hist));
    
    history_append(hist, OP_DELETE, 0, "A", 1);
    TEST_ASSERT_EQUAL_UINT(2, history_count(hist));
    
    history_append(hist, OP_INSERT, 0, "B", 1);
    TEST_ASSERT_EQUAL_UINT(3, history_count(hist));
}

/* ============================================================================
 * Export Tests
 * ============================================================================ */

void test_history_export(void)
{
    history_append(hist, OP_INSERT, 0, "Hello", 5);
    history_append(hist, OP_DELETE, 2, "ll", 2);
    
    const char *export_path = "test_export.txt";
    int result = history_export(hist, export_path);
    TEST_ASSERT_EQUAL_INT(0, result);
    
    /* Check export file exists */
    FILE *f = fopen(export_path, "r");
    TEST_ASSERT_NOT_NULL(f);
    if (f) fclose(f);
    
    unlink(export_path);
}

void test_history_export_null_history(void)
{
    int result = history_export(NULL, "test.txt");
    TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_history_export_null_path(void)
{
    int result = history_export(hist, NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

/* ============================================================================
 * Reload Tests
 * ============================================================================ */

void test_history_reload(void)
{
    history_append(hist, OP_INSERT, 0, "Hello", 5);
    
    int result = history_reload(hist);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(1, history_count(hist));
}

void test_history_reload_null(void)
{
    int result = history_reload(NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

/* ============================================================================
 * Compact Tests
 * ============================================================================ */

void test_history_compact(void)
{
    history_append(hist, OP_INSERT, 0, "A", 1);
    history_append(hist, OP_INSERT, 1, "B", 1);
    
    int result = history_compact(hist, NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(0, history_count(hist));  /* Compact clears */
}

void test_history_compact_with_archive(void)
{
    history_append(hist, OP_INSERT, 0, "Test", 4);
    
    const char *archive = "test_archive.history";
    int result = history_compact(hist, archive);
    TEST_ASSERT_EQUAL_INT(0, result);
    
    /* Check archive exists */
    FILE *f = fopen(archive, "rb");
    TEST_ASSERT_NOT_NULL(f);
    if (f) fclose(f);
    
    unlink(archive);
}

void test_history_compact_null(void)
{
    int result = history_compact(NULL, NULL);
    TEST_ASSERT_EQUAL_INT(-1, result);
}

/* ============================================================================
 * Edge Cases
 * ============================================================================ */

void test_history_large_data(void)
{
    /* Test with larger data chunks */
    char *big_data = malloc(10000);
    TEST_ASSERT_NOT_NULL(big_data);
    memset(big_data, 'X', 9999);
    big_data[9999] = '\0';
    
    int result = history_append(hist, OP_INSERT, 0, big_data, 9999);
    TEST_ASSERT_EQUAL_INT(0, result);
    
    /* Verify it persists */
    history_close(hist);
    hist = history_open(TEST_FILE);
    
    EditOp *op = history_undo(hist);
    TEST_ASSERT_NOT_NULL(op);
    TEST_ASSERT_EQUAL_UINT(9999, op->length);
    
    free(big_data);
}

void test_history_special_characters(void)
{
    /* Test with newlines, tabs, null bytes */
    char special[] = "Line1\nLine2\tTab\r\nCRLF";
    
    history_append(hist, OP_INSERT, 0, special, strlen(special));
    
    EditOp *op = history_undo(hist);
    TEST_ASSERT_NOT_NULL(op);
    TEST_ASSERT_EQUAL_STRING(special, op->data);
}

void test_history_zero_length_data(void)
{
    int result = history_append(hist, OP_INSERT, 0, NULL, 0);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT(1, history_count(hist));
}

void test_history_stress_operations(void)
{
    /* Stress test: many operations */
    for (int i = 0; i < 500; i++) {
        history_append(hist, OP_INSERT, i, "X", 1);
    }
    
    TEST_ASSERT_EQUAL_UINT(500, history_count(hist));
    
    /* Undo all */
    while (history_can_undo(hist)) {
        EditOp *op = history_undo(hist);
        TEST_ASSERT_NOT_NULL(op);
    }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    UNITY_BEGIN();
    
    /* Path Generation */
    RUN_TEST(test_history_get_path);
    RUN_TEST(test_history_get_path_long);
    RUN_TEST(test_history_get_path_buffer_too_small);
    
    /* Timestamp */
    RUN_TEST(test_history_timestamp_nonzero);
    RUN_TEST(test_history_timestamp_increases);
    
    /* Creation and Opening */
    RUN_TEST(test_history_open_creates_file);
    RUN_TEST(test_history_open_empty);
    RUN_TEST(test_history_open_file_size);
    RUN_TEST(test_history_close_null_safe);
    
    /* Append */
    RUN_TEST(test_history_append_insert);
    RUN_TEST(test_history_append_delete);
    RUN_TEST(test_history_append_multiple);
    RUN_TEST(test_history_append_null_returns_error);
    RUN_TEST(test_history_append_increases_size);
    
    /* Undo */
    RUN_TEST(test_history_can_undo_empty);
    RUN_TEST(test_history_can_undo_after_append);
    RUN_TEST(test_history_undo_returns_op);
    RUN_TEST(test_history_undo_multiple);
    RUN_TEST(test_history_undo_empty_returns_null);
    RUN_TEST(test_history_undo_exhausted);
    
    /* Redo */
    RUN_TEST(test_history_can_redo_empty);
    RUN_TEST(test_history_can_redo_after_undo);
    RUN_TEST(test_history_redo_returns_op);
    RUN_TEST(test_history_undo_redo_cycle);
    RUN_TEST(test_history_append_clears_redo);
    
    /* Persistence (Crash Recovery) */
    RUN_TEST(test_history_persistence_reload);
    RUN_TEST(test_history_persistence_data_integrity);
    RUN_TEST(test_history_persistence_many_ops);
    
    /* Clear */
    RUN_TEST(test_history_clear);
    RUN_TEST(test_history_clear_resets_undo_redo);
    RUN_TEST(test_history_clear_null_safe);
    
    /* Count and Size */
    RUN_TEST(test_history_count_null);
    RUN_TEST(test_history_size_null);
    RUN_TEST(test_history_count_accurate);
    
    /* Export */
    RUN_TEST(test_history_export);
    RUN_TEST(test_history_export_null_history);
    RUN_TEST(test_history_export_null_path);
    
    /* Reload */
    RUN_TEST(test_history_reload);
    RUN_TEST(test_history_reload_null);
    
    /* Compact */
    RUN_TEST(test_history_compact);
    RUN_TEST(test_history_compact_with_archive);
    RUN_TEST(test_history_compact_null);
    
    /* Edge Cases */
    RUN_TEST(test_history_large_data);
    RUN_TEST(test_history_special_characters);
    RUN_TEST(test_history_zero_length_data);
    RUN_TEST(test_history_stress_operations);
    
    return UNITY_END();
}
