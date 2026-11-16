/**
 * PulseMetrics - Systems Programming Project
 * Module: Aggregator
 * File: core/include/aggregator.h
 * 
 * Computes window aggregates (sum, avg, min, max, count, rate) from in-memory metric snapshots.
 */

#ifndef PULSE_AGGREGATOR_H
#define PULSE_AGGREGATOR_H

#include "metrics.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[PULSE_MAX_NAME_LEN + 1];
    MetricType type;
    double sum;
    double avg;
    double min;
    double max;
    uint64_t count;
    double rate;
    char window_start[32]; /* ISO 8601 "YYYY-MM-DD HH:MM:SS" */
    char window_end[32];
} AggregationRecord;

/**
 * Computes aggregation records from an array of metric snapshots.
 * Returns the number of generated records.
 */
size_t aggregator_compute(const MetricSnapshot *snapshots, 
                          size_t snapshot_count, 
                          double window_duration_seconds,
                          const char *window_start_iso,
                          const char *window_end_iso,
                          AggregationRecord *out_records, 
                          size_t max_records);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_AGGREGATOR_H */

