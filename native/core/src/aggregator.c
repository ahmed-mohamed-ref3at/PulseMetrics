/**
 * PulseMetrics - Systems Programming Project
 * Module: Aggregator
 * File: core/src/aggregator.c
 * 
 * Computes windowed aggregations for Counters, Gauges, and Timers.
 * Empty windows (count == 0) produce no aggregation records to avoid
 * polluting the database with zero-filled historical rows.
 */

#include "aggregator.h"
#include <string.h>

size_t aggregator_compute(const MetricSnapshot *snapshots, 
                          size_t snapshot_count, 
                          double window_duration_seconds,
                          const char *window_start_iso,
                          const char *window_end_iso,
                          AggregationRecord *out_records, 
                          size_t max_records) {
    if (!snapshots || !out_records || snapshot_count == 0 || max_records == 0) {
        return 0;
    }

    if (window_duration_seconds <= 0.0) {
        window_duration_seconds = 1.0;
    }

    size_t out_count = 0;

    for (size_t i = 0; i < snapshot_count; i++) {
        const MetricSnapshot *s = &snapshots[i];

        /* Determine whether any sample arrived during this window */
        uint64_t sample_count = 0;
        if (s->type == METRIC_TYPE_COUNTER) {
            sample_count = s->data.counter.count;
        } else if (s->type == METRIC_TYPE_GAUGE) {
            sample_count = s->data.gauge.count;
        } else if (s->type == METRIC_TYPE_TIMER) {
            sample_count = s->data.timer.count;
        }

        /* 
         * Rule: Do NOT write an aggregation row for a counter, timer, or gauge
         * whose count is 0 in a window. Only write if a new value arrived.
         */
        if (sample_count == 0) {
            continue;
        }

        if (out_count >= max_records) {
            break;
        }

        AggregationRecord *rec = &out_records[out_count++];

        strncpy_s(rec->name, sizeof(rec->name), s->name, _TRUNCATE);
        rec->type = s->type;
        strncpy_s(rec->window_start, sizeof(rec->window_start), window_start_iso ? window_start_iso : "", _TRUNCATE);
        strncpy_s(rec->window_end, sizeof(rec->window_end), window_end_iso ? window_end_iso : "", _TRUNCATE);

        switch (s->type) {
            case METRIC_TYPE_COUNTER:
                rec->sum = s->data.counter.sum;
                rec->count = s->data.counter.count;
                rec->min = 0.0;
                rec->max = s->data.counter.sum;
                rec->avg = (rec->count > 0) ? (rec->sum / (double)rec->count) : 0.0;
                rec->rate = rec->sum / window_duration_seconds;
                break;

            case METRIC_TYPE_GAUGE:
                rec->count = s->data.gauge.count;
                rec->min = s->data.gauge.min;
                rec->max = s->data.gauge.max;
                rec->avg = (rec->count > 0) ? (s->data.gauge.sum / (double)rec->count) : s->data.gauge.current;
                rec->sum = s->data.gauge.current;
                rec->rate = 0.0;
                break;

            case METRIC_TYPE_TIMER:
                rec->count = s->data.timer.count;
                rec->sum = s->data.timer.sum;
                rec->min = s->data.timer.min;
                rec->max = s->data.timer.max;
                rec->avg = (rec->count > 0) ? (rec->sum / (double)rec->count) : 0.0;
                rec->rate = (double)rec->count / window_duration_seconds;
                break;

            default:
                rec->sum = 0.0;
                rec->avg = 0.0;
                rec->min = 0.0;
                rec->max = 0.0;
                rec->count = 0;
                rec->rate = 0.0;
                break;
        }
    }

    return out_count;
}
