// SPDX-License-Identifier: GPL-2.0
/*
 * xe_top - Unit tests for ring buffer
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../src/util/ring.h"

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) do { tests_total++; printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)

static void test_ring_basic(void)
{
    TEST("ring push/get basic");
    double buf[8];
    ring_buf_t r;
    ring_init(&r, buf, 8);

    ring_push(&r, 1.0);
    ring_push(&r, 2.0);
    ring_push(&r, 3.0);

    if (r.count == 3 && ring_get(&r, 0) == 1.0 && ring_get(&r, 2) == 3.0)
        PASS();
    else
        FAIL("count or values mismatch");
}

static void test_ring_wrap(void)
{
    TEST("ring wrap-around");
    double buf[4];
    ring_buf_t r;
    ring_init(&r, buf, 4);

    for (int i = 0; i < 6; i++)
        ring_push(&r, (double)i);

    /* Should have last 4 values: 2, 3, 4, 5 */
    if (r.count == 4 && ring_get(&r, 0) == 2.0 && ring_get(&r, 3) == 5.0)
        PASS();
    else
        FAIL("wrap-around values mismatch");
}

static void test_ring_latest(void)
{
    TEST("ring latest");
    double buf[8];
    ring_buf_t r;
    ring_init(&r, buf, 8);

    ring_push(&r, 10.0);
    ring_push(&r, 20.0);

    if (ring_latest(&r) == 20.0)
        PASS();
    else
        FAIL("latest value mismatch");
}

static void test_ring_min_max(void)
{
    TEST("ring min/max");
    double buf[8];
    ring_buf_t r;
    ring_init(&r, buf, 8);

    ring_push(&r, 3.0);
    ring_push(&r, 1.0);
    ring_push(&r, 5.0);
    ring_push(&r, 2.0);

    if (ring_min(&r) == 1.0 && ring_max(&r) == 5.0)
        PASS();
    else
        FAIL("min/max mismatch");
}

static void test_ring_empty(void)
{
    TEST("ring empty access");
    double buf[8];
    ring_buf_t r;
    ring_init(&r, buf, 8);

    if (ring_latest(&r) == 0.0 && ring_min(&r) == 0.0 && ring_max(&r) == 0.0)
        PASS();
    else
        FAIL("empty ring should return 0.0");
}

int main(void)
{
    printf("=== xe_top ring buffer tests ===\n\n");

    test_ring_basic();
    test_ring_wrap();
    test_ring_latest();
    test_ring_min_max();
    test_ring_empty();

    printf("\nResults: %d/%d passed\n", tests_passed, tests_total);
    return (tests_passed == tests_total) ? 0 : 1;
}