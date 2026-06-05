// SPDX-License-Identifier: GPL-2.0
/*
 * xe_top - Unit tests for common utilities
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../src/util/common.h"

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) do { \
    tests_total++; \
    printf("  TEST: %s ... ", name); \
} while(0)

#define PASS() do { \
    tests_passed++; \
    printf("PASS\n"); \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
} while(0)

static void test_delta_safe_normal(void)
{
    TEST("DELTA_SAFE normal delta");
    unsigned long long cur = 1000, prev = 500;
    long long d = DELTA_SAFE(cur, prev);
    if (d == 500) { PASS(); }
    else { FAIL("expected 500"); }
}

static void test_delta_safe_zero(void)
{
    TEST("DELTA_SAFE zero delta");
    unsigned long long cur = 500, prev = 500;
    long long d = DELTA_SAFE(cur, prev);
    if (d == 0) { PASS(); }
    else { FAIL("expected 0"); }
}

static void test_delta_safe_wraparound(void)
{
    TEST("DELTA_SAFE counter reset (cur < prev)");
    /* Simulates a counter reset where cur < prev, causing
     * unsigned subtraction to wrap to a huge value that
     * becomes negative when cast to signed long long. */
    unsigned long long cur = 100, prev = 200;
    long long d = DELTA_SAFE(cur, prev);
    if (d == 0) { PASS(); }
    else { FAIL("expected 0 for underflow"); }
}

static void test_delta_safe_small_wrap(void)
{
    TEST("DELTA_SAFE small values");
    unsigned long long cur = 10, prev = 5;
    long long d = DELTA_SAFE(cur, prev);
    if (d == 5) { PASS(); }
    else { FAIL("expected 5"); }
}

static void test_clamp(void)
{
    TEST("clamp_ll in range");
    long long v = clamp_ll(50, 0, 100);
    if (v == 50) { PASS(); }
    else { FAIL("expected 50"); }
}

static void test_clamp_low(void)
{
    TEST("clamp_ll below range");
    long long v = clamp_ll(-10, 0, 100);
    if (v == 0) { PASS(); }
    else { FAIL("expected 0"); }
}

static void test_clamp_high(void)
{
    TEST("clamp_ll above range");
    long long v = clamp_ll(150, 0, 100);
    if (v == 100) { PASS(); }
    else { FAIL("expected 100"); }
}

static void test_min_max(void)
{
    TEST("MIN macro");
    if (MIN(3, 7) == 3) { PASS(); }
    else { FAIL("expected 3"); }
}

static void test_max_macro(void)
{
    TEST("MAX macro");
    if (MAX(3, 7) == 7) { PASS(); }
    else { FAIL("expected 7"); }
}

static void test_array_size(void)
{
    TEST("ARRAY_SIZE macro");
    int arr[10];
    if (ARRAY_SIZE(arr) == 10) { PASS(); }
    else { FAIL("expected 10"); }
}

int main(void)
{
    printf("=== xe_top common utility tests ===\n\n");

    test_delta_safe_normal();
    test_delta_safe_zero();
    test_delta_safe_wraparound();
    test_delta_safe_small_wrap();
    test_clamp();
    test_clamp_low();
    test_clamp_high();
    test_min_max();
    test_max_macro();
    test_array_size();

    printf("\nResults: %d/%d passed\n", tests_passed, tests_total);
    return (tests_passed == tests_total) ? 0 : 1;
}