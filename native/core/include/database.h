/**
 * PulseMetrics - Systems Programming Project
 * Module: Database Writer (SQLite)
 * File: core/include/database.h
 * 
 * Provides database initialization, schema setup (WAL mode), and prepared write operations.
 */

#ifndef PULSE_DATABASE_H
#define PULSE_DATABASE_H

#include "sqlite3.h"
#include "aggregator.h"
#include "alerts.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DatabaseWriter DatabaseWriter;

/**
 * Opens or creates SQLite database, applies WAL mode, and initializes tables.
 */
DatabaseWriter *db_writer_open(const char *db_path);

/**
 * Resolves or registers a metric name and type, returning its primary key (ID).
 */
int64_t db_writer_get_or_create_metric(DatabaseWriter *writer, const char *name, MetricType type);

/**
 * Inserts a raw metric value into 'metric_values'.
 */
bool db_writer_insert_raw_value(DatabaseWriter *writer, int64_t metric_id, double value);

/**
 * Inserts an aggregation window record into 'aggregations'.
 */
bool db_writer_insert_aggregation(DatabaseWriter *writer, int64_t metric_id, const AggregationRecord *rec);

/**
 * Inserts an alert event record into 'alerts'.
 */
bool db_writer_insert_alert(DatabaseWriter *writer, int64_t metric_id, const AlertEvent *alert);

/**
 * Begins an explicit SQLite transaction (BEGIN IMMEDIATE).
 */
bool db_writer_begin_transaction(DatabaseWriter *writer);

/**
 * Commits the current SQLite transaction (COMMIT).
 */
bool db_writer_commit_transaction(DatabaseWriter *writer);

/**
 * Rollback the current SQLite transaction if an error occurred.
 */
bool db_writer_rollback_transaction(DatabaseWriter *writer);

/**
 * Marks active alerts for a given metric and alert type as resolved.
 */
bool db_writer_resolve_alert(DatabaseWriter *writer, int64_t metric_id, const char *alert_type);

/**
 * Closes prepared statements and the database connection.
 */
void db_writer_close(DatabaseWriter *writer);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_DATABASE_H */

