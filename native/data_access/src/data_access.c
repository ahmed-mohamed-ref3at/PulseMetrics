/**
 * PulseMetrics - Systems Programming Project
 * Module: Read-Only Data Access Layer (Plain C)
 * File: data_access/src/data_access.c
 */

#include "data_access.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct DataAccess {
    sqlite3 *db;
    sqlite3_stmt *stmt_metrics;
    sqlite3_stmt *stmt_latest_agg;
    sqlite3_stmt *stmt_range_agg;
    sqlite3_stmt *stmt_active_alerts;
    sqlite3_stmt *stmt_alert_history;
};

DataAccess *dal_open(const char *db_path) {
    if (!db_path) return NULL;

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        if (db) sqlite3_close(db);
        return NULL;
    }

    DataAccess *dal = (DataAccess *)calloc(1, sizeof(DataAccess));
    if (!dal) {
        sqlite3_close(db);
        return NULL;
    }
    dal->db = db;

    /* Precompile queries */
    sqlite3_prepare_v2(db, "SELECT id, name, type, created_at FROM metrics ORDER BY name ASC;", 
                       -1, &dal->stmt_metrics, NULL);

    sqlite3_prepare_v2(db, 
        "SELECT a.id, a.metric_id, m.name, m.type, a.sum, a.avg, a.min, a.max, a.count, a.rate, a.window_start, a.window_end "
        "FROM aggregations a JOIN metrics m ON a.metric_id = m.id "
        "WHERE m.name = ? AND a.count > 0 ORDER BY a.id DESC LIMIT ?;", 
        -1, &dal->stmt_latest_agg, NULL);

    sqlite3_prepare_v2(db,
        "SELECT a.id, a.metric_id, m.name, m.type, a.sum, a.avg, a.min, a.max, a.count, a.rate, a.window_start, a.window_end "
        "FROM aggregations a JOIN metrics m ON a.metric_id = m.id "
        "WHERE m.name = ? AND a.window_end >= ? AND a.window_end <= ? AND a.count > 0 ORDER BY a.id ASC;",
        -1, &dal->stmt_range_agg, NULL);

    sqlite3_prepare_v2(db, 
        "SELECT a.id, COALESCE(m.name, 'system'), a.alert_type, a.threshold, a.actual_value, a.message, a.triggered_at, a.resolved "
        "FROM alerts a LEFT JOIN metrics m ON a.metric_id = m.id "
        "WHERE a.resolved = 0 ORDER BY a.id DESC;", 
        -1, &dal->stmt_active_alerts, NULL);

    sqlite3_prepare_v2(db, 
        "SELECT a.id, COALESCE(m.name, 'system'), a.alert_type, a.threshold, a.actual_value, a.message, a.triggered_at, a.resolved "
        "FROM alerts a LEFT JOIN metrics m ON a.metric_id = m.id "
        "ORDER BY a.id DESC LIMIT ?;", 
        -1, &dal->stmt_alert_history, NULL);

    return dal;
}

bool dal_get_metrics(DataAccess *dal, MetricInfo **out_metrics, size_t *out_count) {
    if (!dal || !dal->stmt_metrics || !out_metrics || !out_count) return false;

    *out_metrics = NULL;
    *out_count = 0;

    sqlite3_reset(dal->stmt_metrics);

    size_t capacity = 16;
    MetricInfo *arr = (MetricInfo *)malloc(capacity * sizeof(MetricInfo));
    if (!arr) return false;

    size_t count = 0;
    while (sqlite3_step(dal->stmt_metrics) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            MetricInfo *temp = (MetricInfo *)realloc(arr, capacity * sizeof(MetricInfo));
            if (!temp) { free(arr); return false; }
            arr = temp;
        }

        MetricInfo *m = &arr[count++];
        m->id = sqlite3_column_int64(dal->stmt_metrics, 0);
        const char *name = (const char *)sqlite3_column_text(dal->stmt_metrics, 1);
        const char *type = (const char *)sqlite3_column_text(dal->stmt_metrics, 2);
        const char *time = (const char *)sqlite3_column_text(dal->stmt_metrics, 3);

        strncpy_s(m->name, sizeof(m->name), name ? name : "", _TRUNCATE);
        strncpy_s(m->type, sizeof(m->type), type ? type : "", _TRUNCATE);
        strncpy_s(m->created_at, sizeof(m->created_at), time ? time : "", _TRUNCATE);
    }

    *out_metrics = arr;
    *out_count = count;
    return true;
}

static bool read_aggregations_from_stmt(sqlite3_stmt *stmt, size_t initial_cap,
                                        AggregationData **out_data, size_t *out_count) {
    *out_data = NULL;
    *out_count = 0;

    size_t capacity = (initial_cap > 0) ? initial_cap : 32;
    AggregationData *arr = (AggregationData *)malloc(capacity * sizeof(AggregationData));
    if (!arr) return false;

    size_t count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            AggregationData *temp = (AggregationData *)realloc(arr, capacity * sizeof(AggregationData));
            if (!temp) { free(arr); return false; }
            arr = temp;
        }

        AggregationData *rec = &arr[count++];
        rec->id = sqlite3_column_int64(stmt, 0);
        rec->metric_id = sqlite3_column_int64(stmt, 1);
        const char *name = (const char *)sqlite3_column_text(stmt, 2);
        const char *type = (const char *)sqlite3_column_text(stmt, 3);
        rec->sum = sqlite3_column_double(stmt, 4);
        rec->avg = sqlite3_column_double(stmt, 5);
        rec->min = sqlite3_column_double(stmt, 6);
        rec->max = sqlite3_column_double(stmt, 7);
        rec->count = (uint64_t)sqlite3_column_int64(stmt, 8);
        rec->rate = sqlite3_column_double(stmt, 9);
        const char *w_start = (const char *)sqlite3_column_text(stmt, 10);
        const char *w_end = (const char *)sqlite3_column_text(stmt, 11);

        strncpy_s(rec->metric_name, sizeof(rec->metric_name), name ? name : "", _TRUNCATE);
        strncpy_s(rec->metric_type, sizeof(rec->metric_type), type ? type : "", _TRUNCATE);
        strncpy_s(rec->window_start, sizeof(rec->window_start), w_start ? w_start : "", _TRUNCATE);
        strncpy_s(rec->window_end, sizeof(rec->window_end), w_end ? w_end : "", _TRUNCATE);
    }

    *out_data = arr;
    *out_count = count;
    return true;
}

bool dal_get_latest_aggregations(DataAccess *dal, const char *metric_name, size_t limit, 
                                 AggregationData **out_data, size_t *out_count) {
    if (!dal || !dal->stmt_latest_agg || !metric_name || !out_data || !out_count) return false;

    size_t effective_limit = (limit > 0) ? limit : 50;
    sqlite3_reset(dal->stmt_latest_agg);
    sqlite3_bind_text(dal->stmt_latest_agg, 1, metric_name, -1, SQLITE_STATIC);
    sqlite3_bind_int64(dal->stmt_latest_agg, 2, (sqlite3_int64)effective_limit);

    size_t initial_cap = (effective_limit <= 100) ? effective_limit : 32;
    return read_aggregations_from_stmt(dal->stmt_latest_agg, initial_cap, out_data, out_count);
}

bool dal_get_aggregations_range(DataAccess *dal, const char *metric_name,
                                const char *start_time, const char *end_time,
                                AggregationData **out_data, size_t *out_count) {
    if (!dal || !dal->stmt_range_agg || !metric_name || !start_time || !end_time || !out_data || !out_count) {
        return false;
    }

    sqlite3_reset(dal->stmt_range_agg);
    sqlite3_bind_text(dal->stmt_range_agg, 1, metric_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(dal->stmt_range_agg, 2, start_time, -1, SQLITE_STATIC);
    sqlite3_bind_text(dal->stmt_range_agg, 3, end_time, -1, SQLITE_STATIC);

    return read_aggregations_from_stmt(dal->stmt_range_agg, 32, out_data, out_count);
}

static bool fetch_alerts(sqlite3_stmt *stmt, AlertInfo **out_alerts, size_t *out_count) {
    *out_alerts = NULL;
    *out_count = 0;

    size_t capacity = 16;
    AlertInfo *arr = (AlertInfo *)malloc(capacity * sizeof(AlertInfo));
    if (!arr) return false;

    size_t count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            AlertInfo *temp = (AlertInfo *)realloc(arr, capacity * sizeof(AlertInfo));
            if (!temp) { free(arr); return false; }
            arr = temp;
        }

        AlertInfo *a = &arr[count++];
        a->id = sqlite3_column_int64(stmt, 0);
        const char *m_name = (const char *)sqlite3_column_text(stmt, 1);
        const char *a_type = (const char *)sqlite3_column_text(stmt, 2);
        a->threshold = sqlite3_column_double(stmt, 3);
        a->actual_value = sqlite3_column_double(stmt, 4);
        const char *msg = (const char *)sqlite3_column_text(stmt, 5);
        const char *trig = (const char *)sqlite3_column_text(stmt, 6);
        a->resolved = sqlite3_column_int(stmt, 7);

        strncpy_s(a->metric_name, sizeof(a->metric_name), m_name ? m_name : "", _TRUNCATE);
        strncpy_s(a->alert_type, sizeof(a->alert_type), a_type ? a_type : "", _TRUNCATE);
        strncpy_s(a->message, sizeof(a->message), msg ? msg : "", _TRUNCATE);
        strncpy_s(a->triggered_at, sizeof(a->triggered_at), trig ? trig : "", _TRUNCATE);
    }

    *out_alerts = arr;
    *out_count = count;
    return true;
}

bool dal_get_active_alerts(DataAccess *dal, AlertInfo **out_alerts, size_t *out_count) {
    if (!dal || !dal->stmt_active_alerts) return false;
    sqlite3_reset(dal->stmt_active_alerts);
    return fetch_alerts(dal->stmt_active_alerts, out_alerts, out_count);
}

bool dal_get_alert_history(DataAccess *dal, size_t limit, AlertInfo **out_alerts, size_t *out_count) {
    if (!dal || !dal->stmt_alert_history) return false;
    sqlite3_reset(dal->stmt_alert_history);
    sqlite3_bind_int64(dal->stmt_alert_history, 1, (sqlite3_int64)(limit > 0 ? limit : 50));
    return fetch_alerts(dal->stmt_alert_history, out_alerts, out_count);
}

static uint64_t count_table_rows(sqlite3 *db, const char *sql) {
    sqlite3_stmt *stmt = NULL;
    uint64_t count = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = (uint64_t)sqlite3_column_int64(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

bool dal_get_system_overview(DataAccess *dal, SystemOverview *out) {
    if (!dal || !dal->db || !out) return false;
    memset(out, 0, sizeof(SystemOverview));

    out->total_metrics = count_table_rows(dal->db, "SELECT COUNT(*) FROM metrics;");
    out->total_raw_values = count_table_rows(dal->db, "SELECT COUNT(*) FROM metric_values;");
    out->total_aggregations = count_table_rows(dal->db, "SELECT COUNT(*) FROM aggregations;");
    out->active_alerts = count_table_rows(dal->db, "SELECT COUNT(*) FROM alerts WHERE resolved = 0;");

    /* Latest CPU and RAM (only windows where count > 0) */
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(dal->db, 
        "SELECT a.avg FROM aggregations a JOIN metrics m ON a.metric_id = m.id WHERE m.name = 'system.cpu_usage' AND a.count > 0 ORDER BY a.id DESC LIMIT 1;", 
        -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW) {
            out->latest_cpu = sqlite3_column_double(s, 0);
        }
        sqlite3_finalize(s);
    }

    if (sqlite3_prepare_v2(dal->db, 
        "SELECT a.avg FROM aggregations a JOIN metrics m ON a.metric_id = m.id WHERE m.name = 'system.ram_usage' AND a.count > 0 ORDER BY a.id DESC LIMIT 1;", 
        -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW) {
            out->latest_ram = sqlite3_column_double(s, 0);
        }
        sqlite3_finalize(s);
    }

    return true;
}

void dal_free_metrics(MetricInfo *m) { free(m); }
void dal_free_aggregations(AggregationData *d) { free(d); }
void dal_free_alerts(AlertInfo *a) { free(a); }

void dal_close(DataAccess *dal) {
    if (!dal) return;
    if (dal->stmt_metrics) sqlite3_finalize(dal->stmt_metrics);
    if (dal->stmt_latest_agg) sqlite3_finalize(dal->stmt_latest_agg);
    if (dal->stmt_range_agg) sqlite3_finalize(dal->stmt_range_agg);
    if (dal->stmt_active_alerts) sqlite3_finalize(dal->stmt_active_alerts);
    if (dal->stmt_alert_history) sqlite3_finalize(dal->stmt_alert_history);
    if (dal->db) sqlite3_close(dal->db);
    free(dal);
}

