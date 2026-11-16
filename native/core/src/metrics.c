/**
 * PulseMetrics - Systems Programming Project
 * Module: In-Memory Metric Store
 * File: core/src/metrics.c
 */

#include "metrics.h"
#include <stdlib.h>
#include <string.h>

/* 32-bit FNV-1a Hash function */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)(*str++);
        hash *= 16777619u;
    }
    return hash;
}

MetricsStore *metrics_store_create(void) {
    MetricsStore *store = (MetricsStore *)calloc(1, sizeof(MetricsStore));
    return store;
}

bool metrics_store_update(MetricsStore *store, const ParsedMetric *metric) {
    if (!store || !metric) return false;

    uint32_t bucket_idx = hash_string(metric->name) % METRICS_HASH_BUCKETS;
    MetricNode *curr = store->buckets[bucket_idx];

    /* Search for existing metric node in bucket list */
    while (curr) {
        if (strcmp(curr->name, metric->name) == 0) {
            /* Update existing entry */
            if (curr->type != metric->type) {
                /* Type mismatch: keep original type or update? Keep original registered type */
                return false;
            }

            switch (curr->type) {
                case METRIC_TYPE_COUNTER:
                    curr->data.counter.sum += metric->value;
                    curr->data.counter.count++;
                    break;

                case METRIC_TYPE_GAUGE:
                    curr->data.gauge.current = metric->value;
                    if (curr->data.gauge.count == 0 || metric->value < curr->data.gauge.min) {
                        curr->data.gauge.min = metric->value;
                    }
                    if (curr->data.gauge.count == 0 || metric->value > curr->data.gauge.max) {
                        curr->data.gauge.max = metric->value;
                    }
                    curr->data.gauge.sum += metric->value;
                    curr->data.gauge.count++;
                    break;

                case METRIC_TYPE_TIMER:
                    if (curr->data.timer.count == 0 || metric->value < curr->data.timer.min) {
                        curr->data.timer.min = metric->value;
                    }
                    if (curr->data.timer.count == 0 || metric->value > curr->data.timer.max) {
                        curr->data.timer.max = metric->value;
                    }
                    curr->data.timer.sum += metric->value;
                    curr->data.timer.count++;
                    break;

                default:
                    break;
            }
            return true;
        }
        curr = curr->next;
    }

    /* Not found: allocate and prepend new node to the bucket */
    MetricNode *new_node = (MetricNode *)calloc(1, sizeof(MetricNode));
    if (!new_node) return false;

    strncpy_s(new_node->name, sizeof(new_node->name), metric->name, _TRUNCATE);
    new_node->type = metric->type;

    switch (metric->type) {
        case METRIC_TYPE_COUNTER:
            new_node->data.counter.sum = metric->value;
            new_node->data.counter.count = 1;
            break;

        case METRIC_TYPE_GAUGE:
            new_node->data.gauge.current = metric->value;
            new_node->data.gauge.min = metric->value;
            new_node->data.gauge.max = metric->value;
            new_node->data.gauge.sum = metric->value;
            new_node->data.gauge.count = 1;
            break;

        case METRIC_TYPE_TIMER:
            new_node->data.timer.min = metric->value;
            new_node->data.timer.max = metric->value;
            new_node->data.timer.sum = metric->value;
            new_node->data.timer.count = 1;
            break;

        default:
            free(new_node);
            return false;
    }

    new_node->next = store->buckets[bucket_idx];
    store->buckets[bucket_idx] = new_node;
    store->total_metrics++;

    return true;
}

size_t metrics_store_snapshot_and_reset(MetricsStore *store, 
                                        MetricSnapshot *out_snapshots, 
                                        size_t max_snapshots) {
    if (!store || !out_snapshots || max_snapshots == 0) return 0;

    size_t copied = 0;

    for (size_t b = 0; b < METRICS_HASH_BUCKETS && copied < max_snapshots; b++) {
        MetricNode *node = store->buckets[b];
        while (node && copied < max_snapshots) {
            /* Copy current state into snapshot */
            strncpy_s(out_snapshots[copied].name, sizeof(out_snapshots[copied].name), node->name, _TRUNCATE);
            out_snapshots[copied].type = node->type;
            out_snapshots[copied].data = node->data;
            copied++;

            /* Reset accumulator values for the next window */
            switch (node->type) {
                case METRIC_TYPE_COUNTER:
                    node->data.counter.sum = 0.0;
                    node->data.counter.count = 0;
                    break;

                case METRIC_TYPE_GAUGE:
                    /* Keep current point-in-time value, reset window statistics for the next window */
                    node->data.gauge.sum = 0.0;
                    node->data.gauge.count = 0;
                    node->data.gauge.min = 0.0;
                    node->data.gauge.max = 0.0;
                    break;

                case METRIC_TYPE_TIMER:
                    node->data.timer.sum = 0.0;
                    node->data.timer.count = 0;
                    node->data.timer.min = 0.0;
                    node->data.timer.max = 0.0;
                    break;

                default:
                    break;
            }

            node = node->next;
        }
    }

    return copied;
}

size_t metrics_store_get_count(const MetricsStore *store) {
    return store ? store->total_metrics : 0;
}

void metrics_store_destroy(MetricsStore *store) {
    if (!store) return;

    for (size_t b = 0; b < METRICS_HASH_BUCKETS; b++) {
        MetricNode *curr = store->buckets[b];
        while (curr) {
            MetricNode *temp = curr;
            curr = curr->next;
            free(temp);
        }
        store->buckets[b] = NULL;
    }

    free(store);
}

