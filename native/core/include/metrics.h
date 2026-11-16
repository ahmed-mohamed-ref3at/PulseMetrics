/**
 * PulseMetrics - Systems Programming Project
 * Module: In-Memory Metric Store
 * File: core/include/metrics.h
 * 
 * Provides an in-memory hash map for tracking live Counters, Gauges, and Timers.
 */

#ifndef PULSE_METRICS_H
#define PULSE_METRICS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define METRICS_HASH_BUCKETS 256

typedef struct {
    double sum;
    uint64_t count;
} CounterState;

typedef struct {
    double current;
    double min;
    double max;
    double sum;
    uint64_t count;
} GaugeState;

typedef struct {
    double min;
    double max;
    double sum;
    uint64_t count;
} TimerState;

typedef union {
    CounterState counter;
    GaugeState   gauge;
    TimerState   timer;
} MetricData;

typedef struct {
    char name[PULSE_MAX_NAME_LEN + 1];
    MetricType type;
    MetricData data;
} MetricSnapshot;

typedef struct MetricNode {
    char name[PULSE_MAX_NAME_LEN + 1];
    MetricType type;
    MetricData data;
    struct MetricNode *next;
} MetricNode;

typedef struct {
    MetricNode *buckets[METRICS_HASH_BUCKETS];
    size_t total_metrics;
} MetricsStore;

/**
 * Creates and initializes a new in-memory metric store.
 */
MetricsStore *metrics_store_create(void);

/**
 * Updates a metric value in the store (or inserts a new entry if not existing).
 */
bool metrics_store_update(MetricsStore *store, const ParsedMetric *metric);

/**
 * Takes a snapshot of all metrics and resets window accumulation counters.
 * - Counters: sum and count reset to 0.
 * - Timers: sum, count, min, max reset.
 * - Gauges: 'current' is retained, while window stats reset.
 * Returns total number of metric entries copied into out_snapshots.
 */
size_t metrics_store_snapshot_and_reset(MetricsStore *store, 
                                        MetricSnapshot *out_snapshots, 
                                        size_t max_snapshots);

/**
 * Returns total number of registered unique metrics.
 */
size_t metrics_store_get_count(const MetricsStore *store);

/**
 * Frees all allocated memory in the metrics store.
 */
void metrics_store_destroy(MetricsStore *store);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_METRICS_H */
