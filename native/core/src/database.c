/**
 * PulseMetrics - Systems Programming Project
 * Module: Database Writer (SQLite)
 * File: core/src/database.c
 */

#include "database.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

struct DatabaseWriter {
    sqlite3 *db;
    sqlite3_stmt *stmt_insert_metric;
    sqlite3_stmt *stmt_select_metric_id;
    sqlite3_stmt *stmt_insert_raw_val;
    sqlite3_stmt *stmt_insert_agg;
    sqlite3_stmt *stmt_insert_alert;
    sqlite3_stmt *stmt_resolve_alert;
};

static const char *SCHEMA_SQL = 
    "PRAGMA journal_mode = WAL;\n"
    "PRAGMA synchronous = NORMAL;\n"
    "CREATE TABLE IF NOT EXISTS metrics (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    name TEXT NOT NULL UNIQUE,\n"
    "    type TEXT NOT NULL CHECK(type IN ('counter', 'gauge', 'timer')),\n"
    "    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS metric_values (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    metric_id INTEGER NOT NULL REFERENCES metrics(id) ON DELETE CASCADE,\n"
    "    value REAL NOT NULL,\n"
    "    received_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS aggregations (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    metric_id INTEGER NOT NULL REFERENCES metrics(id) ON DELETE CASCADE,\n"
    "    sum REAL,\n"
    "    avg REAL,\n"
    "    min REAL,\n"
    "    max REAL,\n"
    "    count INTEGER,\n"
    "    rate REAL,\n"
    "    window_start TEXT NOT NULL,\n"
    "    window_end TEXT NOT NULL\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS alerts (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    metric_id INTEGER REFERENCES metrics(id) ON DELETE SET NULL,\n"
    "    alert_type TEXT NOT NULL,\n"
    "    threshold REAL,\n"
    "    actual_value REAL,\n"
    "    message TEXT NOT NULL,\n"
    "    triggered_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,\n"
    "    resolved INTEGER NOT NULL DEFAULT 0\n"
    ");\n"
    "CREATE INDEX IF NOT EXISTS idx_values_lookup       ON metric_values(metric_id, received_at);\n"
    "CREATE INDEX IF NOT EXISTS idx_aggregations_lookup ON aggregations(metric_id, window_end);\n"
    "CREATE INDEX IF NOT EXISTS idx_alerts_active       ON alerts(resolved, triggered_at);\n";

static void ensure_parent_directory(const char *path) {
    char dir[PULSE_MAX_PATH_LEN];
    strncpy_s(dir, sizeof(dir), path, _TRUNCATE);
    for (char *p = dir; *p; p++) {
        if (*p == '/' || *p == '\\') {
            if (p > dir && p[-1] != ':') {
                char saved = *p;
                *p = '\0';
                CreateDirectoryA(dir, NULL);
                *p = saved;
            }
        }
    }
}

DatabaseWriter *db_writer_open(const char *db_path) {
    if (!db_path) return NULL;

    ensure_parent_directory(db_path);

    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2(db_path, &db, 
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 
                             NULL);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to open SQLite database '%s': %s", db_path, sqlite3_errmsg(db));
        if (db) sqlite3_close(db);
        return NULL;
    }

    /* Execute Schema DDL and WAL configuration */
    char *err_msg = NULL;
    rc = sqlite3_exec(db, SCHEMA_SQL, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to apply database schema: %s", err_msg ? err_msg : "unknown error");
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return NULL;
    }

    DatabaseWriter *writer = (DatabaseWriter *)calloc(1, sizeof(DatabaseWriter));
    if (!writer) {
        sqlite3_close(db);
        return NULL;
    }
    writer->db = db;

    /* Prepare reusable statements */
    sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO metrics (name, type) VALUES (?, ?);", -1, &writer->stmt_insert_metric, NULL);
    sqlite3_prepare_v2(db, "SELECT id FROM metrics WHERE name = ?;", -1, &writer->stmt_select_metric_id, NULL);
    sqlite3_prepare_v2(db, "INSERT INTO metric_values (metric_id, value) VALUES (?, ?);", -1, &writer->stmt_insert_raw_val, NULL);
    sqlite3_prepare_v2(db, 
        "INSERT INTO aggregations (metric_id, sum, avg, min, max, count, rate, window_start, window_end) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);", -1, &writer->stmt_insert_agg, NULL);
    sqlite3_prepare_v2(db, 
        "INSERT INTO alerts (metric_id, alert_type, threshold, actual_value, message, triggered_at, resolved) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);", -1, &writer->stmt_insert_alert, NULL);
    sqlite3_prepare_v2(db,
        "UPDATE alerts SET resolved = 1 WHERE metric_id = ? AND alert_type = ? AND resolved = 0;",
        -1, &writer->stmt_resolve_alert, NULL);

    LOG_INFO("SQLite database initialized successfully (WAL mode enabled) at '%s'", db_path);
    return writer;
}

int64_t db_writer_get_or_create_metric(DatabaseWriter *writer, const char *name, MetricType type) {
    if (!writer || !writer->db || !name) return -1;

    /* Step 1: Insert or ignore */
    sqlite3_reset(writer->stmt_insert_metric);
    sqlite3_bind_text(writer->stmt_insert_metric, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(writer->stmt_insert_metric, 2, metric_type_to_string(type), -1, SQLITE_STATIC);
    sqlite3_step(writer->stmt_insert_metric);

    /* Step 2: Query ID */
    sqlite3_reset(writer->stmt_select_metric_id);
    sqlite3_bind_text(writer->stmt_select_metric_id, 1, name, -1, SQLITE_STATIC);

    int64_t id = -1;
    if (sqlite3_step(writer->stmt_select_metric_id) == SQLITE_ROW) {
        id = sqlite3_column_int64(writer->stmt_select_metric_id, 0);
    }
    return id;
}

bool db_writer_insert_raw_value(DatabaseWriter *writer, int64_t metric_id, double value) {
    if (!writer || !writer->stmt_insert_raw_val || metric_id <= 0) return false;

    sqlite3_reset(writer->stmt_insert_raw_val);
    sqlite3_bind_int64(writer->stmt_insert_raw_val, 1, metric_id);
    sqlite3_bind_double(writer->stmt_insert_raw_val, 2, value);

    int rc = sqlite3_step(writer->stmt_insert_raw_val);
    return (rc == SQLITE_DONE);
}

bool db_writer_insert_aggregation(DatabaseWriter *writer, int64_t metric_id, const AggregationRecord *rec) {
    if (!writer || !writer->stmt_insert_agg || !rec || metric_id <= 0) return false;

    sqlite3_reset(writer->stmt_insert_agg);
    sqlite3_bind_int64(writer->stmt_insert_agg,  1, metric_id);
    sqlite3_bind_double(writer->stmt_insert_agg, 2, rec->sum);
    sqlite3_bind_double(writer->stmt_insert_agg, 3, rec->avg);
    sqlite3_bind_double(writer->stmt_insert_agg, 4, rec->min);
    sqlite3_bind_double(writer->stmt_insert_agg, 5, rec->max);
    sqlite3_bind_int64(writer->stmt_insert_agg,  6, (sqlite3_int64)rec->count);
    sqlite3_bind_double(writer->stmt_insert_agg, 7, rec->rate);
    sqlite3_bind_text(writer->stmt_insert_agg,   8, rec->window_start, -1, SQLITE_STATIC);
    sqlite3_bind_text(writer->stmt_insert_agg,   9, rec->window_end,   -1, SQLITE_STATIC);

    int rc = sqlite3_step(writer->stmt_insert_agg);
    return (rc == SQLITE_DONE);
}

bool db_writer_insert_alert(DatabaseWriter *writer, int64_t metric_id, const AlertEvent *alert) {
    if (!writer || !writer->stmt_insert_alert || !alert) return false;

    sqlite3_reset(writer->stmt_insert_alert);
    if (metric_id > 0) {
        sqlite3_bind_int64(writer->stmt_insert_alert, 1, metric_id);
    } else {
        sqlite3_bind_null(writer->stmt_insert_alert, 1);
    }
    sqlite3_bind_text(writer->stmt_insert_alert,   2, alert->alert_type, -1, SQLITE_STATIC);
    sqlite3_bind_double(writer->stmt_insert_alert, 3, alert->threshold);
    sqlite3_bind_double(writer->stmt_insert_alert, 4, alert->actual_value);
    sqlite3_bind_text(writer->stmt_insert_alert,   5, alert->message, -1, SQLITE_STATIC);
    sqlite3_bind_text(writer->stmt_insert_alert,   6, alert->triggered_at, -1, SQLITE_STATIC);
    sqlite3_bind_int(writer->stmt_insert_alert,    7, alert->resolved);

    int rc = sqlite3_step(writer->stmt_insert_alert);
    return (rc == SQLITE_DONE);
}

bool db_writer_begin_transaction(DatabaseWriter *writer) {
    if (!writer || !writer->db) return false;
    return (sqlite3_exec(writer->db, "BEGIN IMMEDIATE;", NULL, NULL, NULL) == SQLITE_OK);
}

bool db_writer_commit_transaction(DatabaseWriter *writer) {
    if (!writer || !writer->db) return false;
    return (sqlite3_exec(writer->db, "COMMIT;", NULL, NULL, NULL) == SQLITE_OK);
}

bool db_writer_rollback_transaction(DatabaseWriter *writer) {
    if (!writer || !writer->db) return false;
    return (sqlite3_exec(writer->db, "ROLLBACK;", NULL, NULL, NULL) == SQLITE_OK);
}

bool db_writer_resolve_alert(DatabaseWriter *writer, int64_t metric_id, const char *alert_type) {
    if (!writer || !writer->stmt_resolve_alert || !alert_type || metric_id <= 0) return false;

    sqlite3_reset(writer->stmt_resolve_alert);
    sqlite3_bind_int64(writer->stmt_resolve_alert, 1, metric_id);
    sqlite3_bind_text(writer->stmt_resolve_alert, 2, alert_type, -1, SQLITE_STATIC);

    int rc = sqlite3_step(writer->stmt_resolve_alert);
    return (rc == SQLITE_DONE);
}

void db_writer_close(DatabaseWriter *writer) {
    if (!writer) return;

    if (writer->stmt_insert_metric)   sqlite3_finalize(writer->stmt_insert_metric);
    if (writer->stmt_select_metric_id) sqlite3_finalize(writer->stmt_select_metric_id);
    if (writer->stmt_insert_raw_val)  sqlite3_finalize(writer->stmt_insert_raw_val);
    if (writer->stmt_insert_agg)      sqlite3_finalize(writer->stmt_insert_agg);
    if (writer->stmt_insert_alert)    sqlite3_finalize(writer->stmt_insert_alert);
    if (writer->stmt_resolve_alert)   sqlite3_finalize(writer->stmt_resolve_alert);

    if (writer->db) {
        sqlite3_close(writer->db);
    }
    free(writer);
}

