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

#ifndef RING_H
#define RING_H

#include <string.h>

/*
 * ring_buf_t - A simple circular buffer for storing time-series samples
 * for sparkline rendering. Stores double values.
 */
typedef struct
{
    double *buf;
    int capacity;
    int head;   /* next write position */
    int count;  /* number of items stored (<= capacity) */
} ring_buf_t;

/*
 * ring_init - Initialize a ring buffer with external storage.
 */
static inline void ring_init(ring_buf_t *r, double *storage, int capacity)
{
    r->buf = storage;
    r->capacity = capacity;
    r->head = 0;
    r->count = 0;
    memset(storage, 0, sizeof(double) * (unsigned)capacity);
}

/*
 * ring_push - Add a new sample to the ring buffer.
 */
static inline void ring_push(ring_buf_t *r, double value)
{
    r->buf[r->head] = value;
    r->head = (r->head + 1) % r->capacity;
    if (r->count < r->capacity)
    {
        r->count++;
    }
}

/*
 * ring_get - Get the i-th oldest sample (0 = oldest).
 * Returns 0.0 if index is out of range.
 */
static inline double ring_get(const ring_buf_t *r, int index)
{
    if (index < 0 || index >= r->count)
    {
        return 0.0;
    }
    int start = (r->head - r->count + r->capacity) % r->capacity;
    return r->buf[(start + index) % r->capacity];
}

/*
 * ring_latest - Get the most recently pushed value.
 */
static inline double ring_latest(const ring_buf_t *r)
{
    if (r->count == 0)
    {
        return 0.0;
    }
    int idx = (r->head - 1 + r->capacity) % r->capacity;
    return r->buf[idx];
}

/*
 * ring_min / ring_max - Get min/max of stored samples.
 */
static inline double ring_min(const ring_buf_t *r)
{
    if (r->count == 0) return 0.0;
    double m = r->buf[0];
    for (int i = 1; i < r->count; i++)
    {
        if (r->buf[i] < m) m = r->buf[i];
    }
    return m;
}

static inline double ring_max(const ring_buf_t *r)
{
    if (r->count == 0) return 0.0;
    double m = r->buf[0];
    for (int i = 1; i < r->count; i++)
    {
        if (r->buf[i] > m) m = r->buf[i];
    }
    return m;
}

#endif /* RING_H */