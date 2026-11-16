/**
 * PulseMetrics - Systems Programming Project
 * Unit Test: In-Memory Metric Store & Hash Table
 * File: tests/test_metrics.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "metrics.h"

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

static void test_counter_accumulation(void) {
    MetricsStore *store = metrics_store_create();
    TEST_ASSERT(store != NULL);

    ParsedMetric m1 = {.name = "api.hits", .value = 1.0, .type = METRIC_TYPE_COUNTER};
    ParsedMetric m2 = {.name = "api.hits", .value = 4.0, .type = METRIC_TYPE_COUNTER};
    ParsedMetric m3 = {.name = "api.hits", .value = 10.0, .type = METRIC_TYPE_COUNTER};

    metrics_store_update(store, &m1);
    metrics_store_update(store, &m2);
    metrics_store_update(store, &m3);

    MetricSnapshot snap[10];
    size_t n = metrics_store_snapshot_and_reset(store, snap, 10);
    TEST_ASSERT(n == 1);
    TEST_ASSERT(strcmp(snap[0].name, "api.hits") == 0);
    TEST_ASSERT(snap[0].type == METRIC_TYPE_COUNTER);
    TEST_ASSERT(fabs(snap[0].data.counter.sum - 15.0) < 1e-6);
    TEST_ASSERT(snap[0].data.counter.count == 3);

    metrics_store_destroy(store);
}

static void test_gauge_window_reset(void) {
    MetricsStore *store = metrics_store_create();
    TEST_ASSERT(store != NULL);

    /* Window 1: Two samples: 10 and 30 */
    ParsedMetric g1 = {.name = "queue.depth", .value = 10.0, .type = METRIC_TYPE_GAUGE};
    ParsedMetric g2 = {.name = "queue.depth", .value = 30.0, .type = METRIC_TYPE_GAUGE};
    metrics_store_update(store, &g1);
    metrics_store_update(store, &g2);

    MetricSnapshot snap1[10];
    size_t n1 = metrics_store_snapshot_and_reset(store, snap1, 10);
    TEST_ASSERT(n1 == 1);
    TEST_ASSERT(snap1[0].data.gauge.count == 2);
    TEST_ASSERT(fabs(snap1[0].data.gauge.current - 30.0) < 1e-6);
    TEST_ASSERT(fabs(snap1[0].data.gauge.min - 10.0) < 1e-6);
    TEST_ASSERT(fabs(snap1[0].data.gauge.max - 30.0) < 1e-6);
    TEST_ASSERT(fabs(snap1[0].data.gauge.sum - 40.0) < 1e-6);

    /* Window 2: Zero samples received -> snapshot reflects count=0, current=30.0 retained */
    MetricSnapshot snap2[10];
    size_t n2 = metrics_store_snapshot_and_reset(store, snap2, 10);
    TEST_ASSERT(n2 == 1);
    TEST_ASSERT(snap2[0].data.gauge.count == 0);
    TEST_ASSERT(fabs(snap2[0].data.gauge.current - 30.0) < 1e-6);

    /* Window 3: One new sample: 50.0 */
    ParsedMetric g3 = {.name = "queue.depth", .value = 50.0, .type = METRIC_TYPE_GAUGE};
    metrics_store_update(store, &g3);
    MetricSnapshot snap3[10];
    size_t n3 = metrics_store_snapshot_and_reset(store, snap3, 10);
    TEST_ASSERT(n3 == 1);
    TEST_ASSERT(snap3[0].data.gauge.count == 1);
    TEST_ASSERT(fabs(snap3[0].data.gauge.current - 50.0) < 1e-6);
    TEST_ASSERT(fabs(snap3[0].data.gauge.min - 50.0) < 1e-6);
    TEST_ASSERT(fabs(snap3[0].data.gauge.max - 50.0) < 1e-6);

    metrics_store_destroy(store);
}

static void test_timer_streaming_stats(void) {
    MetricsStore *store = metrics_store_create();
    TEST_ASSERT(store != NULL);

    ParsedMetric t1 = {.name = "http.latency", .value = 100.0, .type = METRIC_TYPE_TIMER};
    ParsedMetric t2 = {.name = "http.latency", .value = 200.0, .type = METRIC_TYPE_TIMER};
    ParsedMetric t3 = {.name = "http.latency", .value = 300.0, .type = METRIC_TYPE_TIMER};

    metrics_store_update(store, &t1);
    metrics_store_update(store, &t2);
    metrics_store_update(store, &t3);

    MetricSnapshot snap[10];
    size_t n = metrics_store_snapshot_and_reset(store, snap, 10);
    TEST_ASSERT(n == 1);
    TEST_ASSERT(snap[0].data.timer.count == 3);
    TEST_ASSERT(fabs(snap[0].data.timer.min - 100.0) < 1e-6);
    TEST_ASSERT(fabs(snap[0].data.timer.max - 300.0) < 1e-6);
    TEST_ASSERT(fabs(snap[0].data.timer.sum - 600.0) < 1e-6);

    metrics_store_destroy(store);
}

int main(void) {
    printf("[TEST] Running test_metrics...\n");
    test_counter_accumulation();
    test_gauge_window_reset();
    test_timer_streaming_stats();
    printf("[PASS] test_metrics completed successfully!\n");
    return 0;
}
