// SPDX-License-Identifier: GPL-2.0
/*
 * xe_top - Intel Core Ultra Full-Stack Performance Monitoring Tool
 *
 * Copyright (C) 2025
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef COMMON_H
#define COMMON_H

/*
 * DELTA_SAFE - Compute a safe delta between two unsigned counters.
 * Protects against counter wrap-around (underflow) by clamping negative
 * results to 0.
 */
static inline long long delta_safe(unsigned long long cur, unsigned long long prev)
{
    long long d = (long long)(cur - prev);
    return d < 0 ? 0 : d;
}

#define DELTA_SAFE(cur, prev) delta_safe((cur), (prev))

/*
 * CLAMP - Clamp a value between lo and hi.
 */
static inline long long clamp_ll(long long val, long long lo, long long hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

/*
 * ARRAY_SIZE - Get the number of elements in an array.
 */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/*
 * MIN / MAX macros
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif /* COMMON_H */