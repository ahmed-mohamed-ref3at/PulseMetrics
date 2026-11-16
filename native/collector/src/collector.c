/**
 * PulseMetrics - Systems Programming Project
 * Module: Collector Service (Winsock UDP Server & Aggregator Thread)
 * File: collector/src/collector.c
 */

#include "collector.h"
#include "../../core/include/logger.h"
#include "../../core/include/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_SNAPSHOT_BATCH 256
#define UDP_RECV_BUF_SIZE  2048

static void get_current_iso_time(char *out_buf, size_t buf_len) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(out_buf, buf_len, "%04d-%02d-%02d %02d:%02d:%02d",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

/* Worker thread function: handles periodic metric aggregation, alerts, and DB writing */
static DWORD WINAPI aggregator_thread_func(LPVOID lpParam) {
    PulseCollector *c = (PulseCollector *)lpParam;
    DWORD wait_time_ms = (DWORD)(c->config.flush_interval_seconds * 1000);
    if (wait_time_ms < 1000) wait_time_ms = 1000;

    LOG_INFO("Aggregator thread started (flush interval: %ds)", c->config.flush_interval_seconds);

    char window_start[32];
    get_current_iso_time(window_start, sizeof(window_start));

    MetricSnapshot snapshots[MAX_SNAPSHOT_BATCH];
    AggregationRecord records[MAX_SNAPSHOT_BATCH];
    AlertEvent alerts[PULSE_MAX_CUSTOM_ALERTS + 4];
    AlertTracker alert_tracker;
    alert_tracker_init(&alert_tracker);

    while (c->is_running) {
        /* Wait on the shutdown event with timeout equal to the aggregation window */
        DWORD wait_res = WaitForSingleObject(c->shutdown_event, wait_time_ms);

        char window_end[32];
        get_current_iso_time(window_end, sizeof(window_end));

        /* Step 1: Thread-safely snapshot metrics and steal the raw metric queue */
        EnterCriticalSection(&c->store_lock);
        size_t count = metrics_store_snapshot_and_reset(c->store, snapshots, MAX_SNAPSHOT_BATCH);

        RawMetricEntry *raw_to_flush = c->raw_queue;
        size_t raw_to_flush_count = c->raw_queue_count;
        c->raw_queue = NULL;
        c->raw_queue_count = 0;
        c->raw_queue_capacity = 0;
        LeaveCriticalSection(&c->store_lock);

        /* Step 2: Compute aggregations and evaluate alerts */
        size_t agg_count = 0;
        if (count > 0) {
            agg_count = aggregator_compute(snapshots, count, 
                                           (double)c->config.flush_interval_seconds,
                                           window_start, window_end,
                                           records, MAX_SNAPSHOT_BATCH);
        }

        size_t alert_count = 0;
        if (agg_count > 0) {
            alert_count = alerts_evaluate(&alert_tracker, records, agg_count, &c->config, 
                                          window_end, alerts, sizeof(alerts) / sizeof(alerts[0]));
        }

        /* Step 3: Single-Threaded SQLite writes inside ONE transaction per flush */
        if (c->db_writer && (raw_to_flush_count > 0 || agg_count > 0 || alert_count > 0 || count > 0)) {
            db_writer_begin_transaction(c->db_writer);

            /* Ensure all tracked metrics remain registered in metrics table */
            for (size_t i = 0; i < count; i++) {
                db_writer_get_or_create_metric(c->db_writer, snapshots[i].name, snapshots[i].type);
            }

            /* A. Flush raw values collected during this window */
            for (size_t i = 0; i < raw_to_flush_count; i++) {
                int64_t metric_id = db_writer_get_or_create_metric(c->db_writer, raw_to_flush[i].name, raw_to_flush[i].type);
                if (metric_id > 0) {
                    db_writer_insert_raw_value(c->db_writer, metric_id, raw_to_flush[i].value);
                }
            }

            /* B. Flush computed aggregations */
            for (size_t i = 0; i < agg_count; i++) {
                int64_t metric_id = db_writer_get_or_create_metric(c->db_writer, records[i].name, records[i].type);
                if (metric_id > 0) {
                    db_writer_insert_aggregation(c->db_writer, metric_id, &records[i]);
                }
            }

            /* C. Handle stateful alerts */
            for (size_t a = 0; a < alert_count; a++) {
                int64_t metric_id = db_writer_get_or_create_metric(c->db_writer, alerts[a].metric_name, METRIC_TYPE_GAUGE);
                if (alerts[a].status == ALERT_STATUS_NEW) {
                    db_writer_insert_alert(c->db_writer, metric_id, &alerts[a]);
                    LOG_WARN("NEW ALERT TRIGGERED: %s", alerts[a].message);
                } else if (alerts[a].status == ALERT_STATUS_RESOLVED) {
                    db_writer_resolve_alert(c->db_writer, metric_id, alerts[a].alert_type);
                    LOG_INFO("ALERT RESOLVED: %s", alerts[a].message);
                }
            }

            db_writer_commit_transaction(c->db_writer);

            LOG_INFO("Flushed %zu raw values and %zu aggregations to database in one transaction (window: %s to %s)", 
                     raw_to_flush_count, agg_count, window_start, window_end);
        }

        if (raw_to_flush) {
            free(raw_to_flush);
        }

        /* Update window start timestamp for the next cycle */
        strncpy_s(window_start, sizeof(window_start), window_end, _TRUNCATE);

        /* If shutdown was signaled, exit thread loop */
        if (wait_res == WAIT_OBJECT_0) {
            break;
        }
    }

    LOG_INFO("Aggregator thread finished cleanly.");
    return 0;
}

typedef struct {
    PulseCollector *collector;
    size_t processed_in_packet;
} ParseContext;

static void on_metric_parsed(const ParsedMetric *metric, void *user_data) {
    ParseContext *ctx = (ParseContext *)user_data;
    PulseCollector *c = ctx->collector;

    /* Push metric into in-memory hash store and buffer raw value in memory under lock.
     * ZERO SQLite calls are made from this receiver thread! */
    EnterCriticalSection(&c->store_lock);
    metrics_store_update(c->store, metric);

    /* Enqueue raw value for the aggregator thread to drain into SQLite */
    if (c->raw_queue_count >= c->raw_queue_capacity) {
        size_t new_cap = (c->raw_queue_capacity == 0) ? 64 : c->raw_queue_capacity * 2;
        RawMetricEntry *new_q = (RawMetricEntry *)realloc(c->raw_queue, new_cap * sizeof(RawMetricEntry));
        if (new_q) {
            c->raw_queue = new_q;
            c->raw_queue_capacity = new_cap;
        }
    }

    if (c->raw_queue && c->raw_queue_count < c->raw_queue_capacity) {
        RawMetricEntry *e = &c->raw_queue[c->raw_queue_count++];
        strncpy_s(e->name, sizeof(e->name), metric->name, _TRUNCATE);
        e->type = metric->type;
        e->value = metric->value;
    }
    LeaveCriticalSection(&c->store_lock);

    ctx->processed_in_packet++;
    c->total_metrics_processed++;
}

PulseCollector *collector_create(const PulseConfig *config) {
    if (!config) return NULL;

    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (res != 0) {
        LOG_ERROR("WSAStartup failed with error code: %d", res);
        return NULL;
    }

    PulseCollector *c = (PulseCollector *)calloc(1, sizeof(PulseCollector));
    if (!c) {
        WSACleanup();
        return NULL;
    }
    c->config = *config;
    c->is_running = true;

    /* Initialize synchronization primitives */
    InitializeCriticalSection(&c->store_lock);
    InitializeCriticalSection(&c->stop_lock);
    c->shutdown_event = CreateEvent(NULL, TRUE, FALSE, NULL);

    /* Initialize in-memory metric store */
    c->store = metrics_store_create();
    if (!c->store) {
        LOG_ERROR("Failed to allocate in-memory metrics store");
        collector_destroy(c);
        return NULL;
    }

    /* Initialize database writer */
    c->db_writer = db_writer_open(c->config.db_path);
    if (!c->db_writer) {
        LOG_ERROR("Failed to initialize database writer at '%s'", c->config.db_path);
        collector_destroy(c);
        return NULL;
    }

    /* Create UDP Socket */
    c->udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (c->udp_socket == INVALID_SOCKET) {
        LOG_ERROR("Failed to create UDP socket: %d", WSAGetLastError());
        collector_destroy(c);
        return NULL;
    }

    /* Optional: expand socket receive buffer to 1MB to handle bursts */
    int rcvbuf = 1024 * 1024;
    setsockopt(c->udp_socket, SOL_SOCKET, SO_RCVBUF, (const char *)&rcvbuf, sizeof(rcvbuf));

    /* Bind UDP Socket explicitly to loopback 127.0.0.1 */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((u_short)c->config.collector_port);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(c->udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        LOG_ERROR("Failed to bind UDP socket to %s:%d (error: %d)", 
                  c->config.collector_host, c->config.collector_port, WSAGetLastError());
        collector_destroy(c);
        return NULL;
    }

    /* Launch background aggregation worker thread */
    c->aggregator_thread = CreateThread(NULL, 0, aggregator_thread_func, c, 0, NULL);
    if (!c->aggregator_thread) {
        LOG_ERROR("Failed to create aggregator worker thread: %lu", GetLastError());
        collector_destroy(c);
        return NULL;
    }

    LOG_INFO("PulseCollector initialized successfully on %s:%d", c->config.collector_host, c->config.collector_port);
    return c;
}

void collector_run(PulseCollector *c) {
    if (!c || c->udp_socket == INVALID_SOCKET) return;

    char recv_buf[UDP_RECV_BUF_SIZE];
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);

    LOG_INFO("Collector UDP receiver loop running. Ready for incoming metrics...");

    while (c->is_running) {
        client_addr_len = sizeof(client_addr);
        int bytes = recvfrom(c->udp_socket, recv_buf, sizeof(recv_buf) - 1, 0,
                             (struct sockaddr *)&client_addr, &client_addr_len);

        if (bytes == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAEINTR || err == WSAESHUTDOWN || !c->is_running) {
                break; /* Clean break during shutdown */
            }
            LOG_WARN("recvfrom error: %d", err);
            continue;
        }

        if (bytes <= 0) continue;

        c->total_packets_received++;
        recv_buf[bytes] = '\0';
        LOG_INFO("UDP packet received (%d bytes)", bytes);

        ParseContext ctx = { .collector = c, .processed_in_packet = 0 };
        parser_parse_buffer(recv_buf, (size_t)bytes, on_metric_parsed, &ctx);
    }

    LOG_INFO("Collector UDP receiver loop terminated.");
}

void collector_stop(PulseCollector *c) {
    if (!c) return;

    EnterCriticalSection(&c->stop_lock);
    if (!c->is_running) {
        LeaveCriticalSection(&c->stop_lock);
        return;
    }

    LOG_INFO("Initiating graceful collector shutdown...");
    c->is_running = false;

    /* Signal shutdown event to wake aggregator immediately */
    if (c->shutdown_event) {
        SetEvent(c->shutdown_event);
    }

    /* Close socket to unblock recvfrom() */
    if (c->udp_socket != INVALID_SOCKET) {
        closesocket(c->udp_socket);
        c->udp_socket = INVALID_SOCKET;
    }

    /* Wait for aggregator thread to complete final flush */
    if (c->aggregator_thread) {
        WaitForSingleObject(c->aggregator_thread, 5000);
        CloseHandle(c->aggregator_thread);
        c->aggregator_thread = NULL;
    }

    LOG_INFO("Collector shutdown complete. Total packets: %llu, metrics processed: %llu",
             (unsigned long long)c->total_packets_received,
             (unsigned long long)c->total_metrics_processed);
    LeaveCriticalSection(&c->stop_lock);
}

void collector_destroy(PulseCollector *c) {
    if (!c) return;

    collector_stop(c);

    if (c->shutdown_event) {
        CloseHandle(c->shutdown_event);
        c->shutdown_event = NULL;
    }

    if (c->raw_queue) {
        free(c->raw_queue);
        c->raw_queue = NULL;
    }

    DeleteCriticalSection(&c->stop_lock);
    DeleteCriticalSection(&c->store_lock);

    if (c->db_writer) {
        db_writer_close(c->db_writer);
        c->db_writer = NULL;
    }

    if (c->store) {
        metrics_store_destroy(c->store);
        c->store = NULL;
    }

    WSACleanup();
    free(c);
}
