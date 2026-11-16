/**
 * PulseMetrics - Systems Programming Project
 * Module: Collector Service (Winsock UDP Server & Aggregator Thread)
 * File: collector/include/collector.h
 */

#ifndef PULSE_COLLECTOR_H
#define PULSE_COLLECTOR_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdbool.h>

#include "../../core/include/config.h"
#include "../../core/include/metrics.h"
#include "../../core/include/database.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char name[PULSE_MAX_NAME_LEN + 1];
    MetricType type;
    double value;
} RawMetricEntry;

typedef struct {
    PulseConfig config;
    SOCKET udp_socket;
    MetricsStore *store;
    DatabaseWriter *db_writer;
    CRITICAL_SECTION store_lock;
    CRITICAL_SECTION stop_lock;
    HANDLE aggregator_thread;
    HANDLE shutdown_event;
    volatile bool is_running;

    /* In-memory raw metric queue populated by receiver thread, drained by aggregator thread */
    RawMetricEntry *raw_queue;
    size_t raw_queue_count;
    size_t raw_queue_capacity;

    /* Metrics statistics */
    uint64_t total_packets_received;
    uint64_t total_metrics_processed;
} PulseCollector;

/**
 * Initializes the collector, socket, database, and concurrency primitives.
 */
PulseCollector *collector_create(const PulseConfig *config);

/**
 * Starts the UDP receiver loop on the current thread. Blocks until collector_stop() is called.
 */
void collector_run(PulseCollector *collector);

/**
 * Signals shutdown, unblocks socket and threads, and waits for final flush.
 */
void collector_stop(PulseCollector *collector);

/**
 * Cleans up and frees all collector resources.
 */
void collector_destroy(PulseCollector *collector);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_COLLECTOR_H */
