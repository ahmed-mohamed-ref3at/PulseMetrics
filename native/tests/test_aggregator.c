/**
 * PulseMetrics - Systems Programming Project
 * Unit Test: Metric Aggregator
 * File: tests/test_aggregator.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "aggregator.h"

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

static void test_counter_aggregation(void) {
    MetricSnapshot snap;
    memset(&snap, 0, sizeof(snap));
    strncpy_s(snap.name, sizeof(snap.name), "requests", _TRUNCATE);
    snap.type = METRIC_TYPE_COUNTER;
    snap.data.counter.sum = 100.0;
    snap.data.counter.count = 20;

    AggregationRecord rec;
    size_t count = aggregator_compute(&snap, 1, 10.0, "2026-11-12T00:00:00Z", "2026-11-12T00:00:10Z", &rec, 1);
    TEST_ASSERT(count == 1);
    TEST_ASSERT(strcmp(rec.name, "requests") == 0);
    TEST_ASSERT(fabs(rec.sum - 100.0) < 1e-6);
    TEST_ASSERT(rec.count == 20);
    TEST_ASSERT(fabs(rec.rate - 10.0) < 1e-6); /* 100 / 10s = 10.0 ops/s */
    TEST_ASSERT(fabs(rec.avg - 5.0) < 1e-6);   /* 100 / 20 = 5.0 */
}

static void test_gauge_aggregation(void) {
    /* 1. Window with samples */
    MetricSnapshot snap1;
    memset(&snap1, 0, sizeof(snap1));
    strncpy_s(snap1.name, sizeof(snap1.name), "temperature", _TRUNCATE);
    snap1.type = METRIC_TYPE_GAUGE;
    snap1.data.gauge.current = 28.5;
    snap1.data.gauge.min = 20.0;
    snap1.data.gauge.max = 30.0;
    snap1.data.gauge.sum = 50.0;
    snap1.data.gauge.count = 2;

    AggregationRecord rec1;
    size_t c1 = aggregator_compute(&snap1, 1, 10.0, "2026-11-12T00:00:00Z", "2026-11-12T00:00:10Z", &rec1, 1);
    TEST_ASSERT(c1 == 1);
    TEST_ASSERT(fabs(rec1.sum - 28.5) < 1e-6); /* Latest value */
    TEST_ASSERT(fabs(rec1.min - 20.0) < 1e-6);
    TEST_ASSERT(fabs(rec1.max - 30.0) < 1e-6);
    TEST_ASSERT(fabs(rec1.avg - 25.0) < 1e-6); /* 50.0 / 2 */
    TEST_ASSERT(rec1.count == 2);

    /* 2. Window with ZERO samples -> does NOT write an aggregation record */
    MetricSnapshot snap2;
    memset(&snap2, 0, sizeof(snap2));
    strncpy_s(snap2.name, sizeof(snap2.name), "temperature", _TRUNCATE);
    snap2.type = METRIC_TYPE_GAUGE;
    snap2.data.gauge.current = 28.5;
    snap2.data.gauge.count = 0;

    AggregationRecord rec2;
    size_t c2 = aggregator_compute(&snap2, 1, 10.0, "2026-11-12T00:00:10Z", "2026-11-12T00:00:20Z", &rec2, 1);
    TEST_ASSERT(c2 == 0); /* Skipped to prevent empty zero rows */
}

static void test_empty_window_skipped(void) {
    MetricSnapshot snaps[3];
    memset(snaps, 0, sizeof(snaps));

    /* Empty counter (count == 0) */
    strncpy_s(snaps[0].name, sizeof(snaps[0].name), "idle_counter", _TRUNCATE);
    snaps[0].type = METRIC_TYPE_COUNTER;
    snaps[0].data.counter.count = 0;

    /* Empty gauge (count == 0) */
    strncpy_s(snaps[1].name, sizeof(snaps[1].name), "idle_gauge", _TRUNCATE);
    snaps[1].type = METRIC_TYPE_GAUGE;
    snaps[1].data.gauge.count = 0;

    /* Empty timer (count == 0) */
    strncpy_s(snaps[2].name, sizeof(snaps[2].name), "idle_timer", _TRUNCATE);
    snaps[2].type = METRIC_TYPE_TIMER;
    snaps[2].data.timer.count = 0;

    AggregationRecord records[3];
    size_t count = aggregator_compute(snaps, 3, 10.0, "2026-11-12T00:00:00Z", "2026-11-12T00:00:10Z", records, 3);
    TEST_ASSERT(count == 0); /* All empty windows skipped */
}

static void test_timer_aggregation(void) {
    MetricSnapshot snap;
    memset(&snap, 0, sizeof(snap));
    strncpy_s(snap.name, sizeof(snap.name), "db.query", _TRUNCATE);
    snap.type = METRIC_TYPE_TIMER;
    snap.data.timer.count = 4;
    snap.data.timer.sum = 400.0;
    snap.data.timer.min = 50.0;
    snap.data.timer.max = 200.0;

    AggregationRecord rec;
    size_t count = aggregator_compute(&snap, 1, 10.0, "2026-11-12T00:00:00Z", "2026-11-12T00:00:10Z", &rec, 1);
    TEST_ASSERT(count == 1);
    TEST_ASSERT(rec.count == 4);
    TEST_ASSERT(fabs(rec.min - 50.0) < 1e-6);
    TEST_ASSERT(fabs(rec.max - 200.0) < 1e-6);
    TEST_ASSERT(fabs(rec.avg - 100.0) < 1e-6); /* 400 / 4 */
    TEST_ASSERT(fabs(rec.rate - 0.4) < 1e-6);   /* 4 calls / 10s = 0.4 calls/s */
}

int main(void) {
    printf("[TEST] Running test_aggregator...\n");
    test_counter_aggregation();
    test_gauge_aggregation();
    test_empty_window_skipped();
    test_timer_aggregation();
    printf("[PASS] test_aggregator completed successfully!\n");
    return 0;
}

