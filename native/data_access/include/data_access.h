/**
 * PulseMetrics - Systems Programming Project
 * Module: Read-Only Data Access Layer (Plain C)
 * File: data_access/include/data_access.h
 * 
 * Provides read-only query functions for metrics, aggregations, alerts, and system stats.
 * Completely independent of GTK and GUI components.
 */

#ifndef PULSE_DATA_ACCESS_H
#define PULSE_DATA_ACCESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DataAccess DataAccess;

typedef struct {
    int64_t id;
    char name[128];
    char type[16]; /* "counter", "gauge", "timer" */
    char created_at[32];
} MetricInfo;

typedef struct {
    int64_t id;
    int64_t metric_id;
    char metric_name[128];
    char metric_type[16];
    double sum;
    double avg;
    double min;
    double max;
    uint64_t count;
    double rate;
    char window_start[32];
    char window_end[32];
} AggregationData;

typedef struct {
    int64_t id;
    char metric_name[128];
    char alert_type[64];
    double threshold;
    double actual_value;
    char message[256];
    char triggered_at[32];
    int resolved;
} AlertInfo;

typedef struct {
    uint64_t total_metrics;
    uint64_t total_raw_values;
    uint64_t total_aggregations;
    uint64_t active_alerts;
    double latest_cpu;
    double latest_ram;
} SystemOverview;

/**
 * Opens SQLite database in read-only mode.
 */
DataAccess *dal_open(const char *db_path);

/**
 * Retrieves list of all registered metrics.
 */
bool dal_get_metrics(DataAccess *dal, MetricInfo **out_metrics, size_t *out_count);

/**
 * Retrieves the latest N aggregation records for a given metric.
 */
bool dal_get_latest_aggregations(DataAccess *dal, const char *metric_name, size_t limit, 
                                 AggregationData **out_data, size_t *out_count);

/**
 * Retrieves aggregation records within a specified ISO time window.
 */
bool dal_get_aggregations_range(DataAccess *dal, const char *metric_name, 
                                const char *start_time, const char *end_time,
                                AggregationData **out_data, size_t *out_count);

/**
 * Retrieves all currently active (unresolved) alerts.
 */
bool dal_get_active_alerts(DataAccess *dal, AlertInfo **out_alerts, size_t *out_count);

/**
 * Retrieves recent alert history (both active and resolved).
 */
bool dal_get_alert_history(DataAccess *dal, size_t limit, AlertInfo **out_alerts, size_t *out_count);

/**
 * Summarizes global system metrics for high-level dashboard cards.
 */
bool dal_get_system_overview(DataAccess *dal, SystemOverview *out_overview);

/* Memory deallocation helpers */
void dal_free_metrics(MetricInfo *metrics);
void dal_free_aggregations(AggregationData *data);
void dal_free_alerts(AlertInfo *alerts);

/**
 * Closes read-only database connection.
 */
void dal_close(DataAccess *dal);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_DATA_ACCESS_H */

