/**
 * PulseMetrics - Systems Programming Project
 * Integration Test: End-to-End UDP Metric Pipeline & Verification
 * File: tests/test_integration.c
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "../collector/include/collector.h"
#include "../core/include/logger.h"
#include "../core/include/parser.h"
#include "sqlite3.h"

static DWORD WINAPI collector_thread_proc(LPVOID param) {
    collector_run((PulseCollector *)param);
    return 0;
}

int main(void) {
    logger_init(LOG_LEVEL_INFO, stdout);

    printf("=====================================================\n");
    printf("   PulseMetrics - Systems Programming Integration Test\n");
    printf("=====================================================\n");

    /* 1. Setup test config with a dedicated test database */
    PulseConfig config;
    config_set_defaults(&config);
    strncpy_s(config.collector_host, sizeof(config.collector_host), "127.0.0.1", _TRUNCATE);
    config.collector_port = 8129; /* Dedicated test port */
    config.flush_interval_seconds = 2; /* Fast 2s flush for testing */
    strncpy_s(config.db_path, sizeof(config.db_path), "database/test_pulsemetrics.db", _TRUNCATE);

    /* Remove old test db if existing */
    DeleteFileA("database/test_pulsemetrics.db");
    DeleteFileA("database/test_pulsemetrics.db-wal");
    DeleteFileA("database/test_pulsemetrics.db-shm");

    /* 2. Initialize Collector */
    PulseCollector *collector = collector_create(&config);
    assert(collector != NULL && "Failed to create collector instance");

    /* Spawn a client socket to send test packets */
    SOCKET client_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    assert(client_sock != INVALID_SOCKET);

    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons((u_short)config.collector_port);
    target.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* 3. Send test packets */
    printf("[TEST] Sending test metrics over UDP to 127.0.0.1:%d...\n", config.collector_port);

    const char *test_packets[] = {
        "demo.requests:1|c",
        "demo.requests:2|c",
        "demo.requests:5|c",
        "demo.queue_depth:7|g",
        "demo.queue_depth:14|g",
        "demo.response_time:120|ms",
        "demo.response_time:200|ms",
        "demo.response_time:150|ms",
        "batch.items:10|c\nbatch.latency:45.5|ms",
        /* Invalid packets to verify robustness */
        "invalid_packet_without_separators",
        "bad.counter:-10|c",
        ":bad_name:5|g"
    };

    /* Start collector receiver loop in a thread for testing */
    HANDLE recv_thread = CreateThread(NULL, 0, collector_thread_proc, collector, 0, NULL);
    assert(recv_thread != NULL);
    Sleep(200); /* Brief delay to ensure socket is listening */

    size_t packet_count = sizeof(test_packets) / sizeof(test_packets[0]);
    for (size_t i = 0; i < packet_count; i++) {
        int bytes_sent = sendto(client_sock, test_packets[i], (int)strlen(test_packets[i]), 0, 
               (struct sockaddr *)&target, sizeof(target));
        if (bytes_sent == SOCKET_ERROR) {
            printf("  [SEND ERROR] error: %d\n", WSAGetLastError());
        } else {
            printf("  [SENT] %d bytes\n", bytes_sent);
        }
        Sleep(20);
    }

    printf("[TEST] Waiting 3 seconds for aggregation window flush...\n");
    Sleep(3000);

    /* Stop and destroy collector so all transactions and WAL are flushed and closed */
    collector_stop(collector);
    WaitForSingleObject(recv_thread, 3000);
    CloseHandle(recv_thread);
    closesocket(client_sock);
    collector_destroy(collector);
    collector = NULL;

    /* 4. Verify SQLite Database Contents */
    printf("[TEST] Verifying SQLite database results...\n");
    sqlite3 *db = NULL;
    int rc = sqlite3_open_v2("database/test_pulsemetrics.db", &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to open test database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    /* Check Counter aggregation for demo.requests (expected sum: 1 + 2 + 5 = 8) */
    sqlite3_stmt *stmt = NULL;
    const char *q_counter = 
        "SELECT a.sum, a.count, a.rate FROM aggregations a "
        "JOIN metrics m ON a.metric_id = m.id WHERE m.name = 'demo.requests' ORDER BY a.id ASC LIMIT 1;";
    rc = sqlite3_prepare_v2(db, q_counter, -1, &stmt, NULL);
    assert(rc == SQLITE_OK);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        printf("  [FAIL] Missing aggregation row for demo.requests\n");
        return 1;
    }
    double counter_sum = sqlite3_column_double(stmt, 0);
    int counter_count = sqlite3_column_int(stmt, 1);
    printf("  [RESULT] demo.requests -> Sum: %.1f (expected 8.0), Count: %d (expected 3)\n", 
           counter_sum, counter_count);
    if (counter_sum != 8.0 || counter_count != 3) {
        printf("  [FAIL] demo.requests values mismatch!\n");
        return 1;
    }
    sqlite3_finalize(stmt);

    /* Check Gauge aggregation for demo.queue_depth (expected latest: 14, min: 7, max: 14) */
    const char *q_gauge = 
        "SELECT a.sum, a.min, a.max, a.count FROM aggregations a "
        "JOIN metrics m ON a.metric_id = m.id WHERE m.name = 'demo.queue_depth' ORDER BY a.id ASC LIMIT 1;";
    rc = sqlite3_prepare_v2(db, q_gauge, -1, &stmt, NULL);
    if (rc != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW) {
        printf("  [FAIL] Missing aggregation row for demo.queue_depth\n");
        return 1;
    }
    double gauge_val = sqlite3_column_double(stmt, 0);
    double gauge_min = sqlite3_column_double(stmt, 1);
    double gauge_max = sqlite3_column_double(stmt, 2);
    printf("  [RESULT] demo.queue_depth -> Value: %.1f (expected 14.0), Min: %.1f, Max: %.1f\n", 
           gauge_val, gauge_min, gauge_max);
    if (gauge_val != 14.0 || gauge_min != 7.0 || gauge_max != 14.0) {
        printf("  [FAIL] demo.queue_depth values mismatch!\n");
        return 1;
    }
    sqlite3_finalize(stmt);

    /* Check Timer aggregation for demo.response_time (expected count: 3, min: 120, max: 200, sum: 470) */
    const char *q_timer = 
        "SELECT a.count, a.min, a.max, a.avg, a.sum FROM aggregations a "
        "JOIN metrics m ON a.metric_id = m.id WHERE m.name = 'demo.response_time' ORDER BY a.id ASC LIMIT 1;";
    rc = sqlite3_prepare_v2(db, q_timer, -1, &stmt, NULL);
    if (rc != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW) {
        printf("  [FAIL] Missing aggregation row for demo.response_time\n");
        return 1;
    }
    int timer_count = sqlite3_column_int(stmt, 0);
    double timer_min = sqlite3_column_double(stmt, 1);
    double timer_max = sqlite3_column_double(stmt, 2);
    double timer_avg = sqlite3_column_double(stmt, 3);
    double timer_sum = sqlite3_column_double(stmt, 4);
    printf("  [RESULT] demo.response_time -> Count: %d (expected 3), Min: %.1f, Max: %.1f, Avg: %.2f, Sum: %.1f\n",
           timer_count, timer_min, timer_max, timer_avg, timer_sum);
    if (timer_count != 3 || timer_min != 120.0 || timer_max != 200.0 || timer_sum != 470.0) {
        printf("  [FAIL] demo.response_time values mismatch!\n");
        return 1;
    }
    sqlite3_finalize(stmt);

    /* Check Batch parsing (batch.items and batch.latency) */
    const char *q_batch = "SELECT COUNT(*) FROM metrics WHERE name IN ('batch.items', 'batch.latency');";
    rc = sqlite3_prepare_v2(db, q_batch, -1, &stmt, NULL);
    if (rc != SQLITE_OK || sqlite3_step(stmt) != SQLITE_ROW) {
        printf("  [FAIL] Failed to query batch metrics\n");
        return 1;
    }
    int batch_metrics_count = sqlite3_column_int(stmt, 0);
    printf("  [RESULT] Multi-metric batch datagram parsed successfully (%d metrics registered)\n", batch_metrics_count);
    if (batch_metrics_count != 2) {
        printf("  [FAIL] Expected 2 batch metrics, found %d\n", batch_metrics_count);
        return 1;
    }
    sqlite3_finalize(stmt);

    sqlite3_close(db);

    DeleteFileA("database/test_pulsemetrics.db");
    DeleteFileA("database/test_pulsemetrics.db-wal");
    DeleteFileA("database/test_pulsemetrics.db-shm");

    printf("=====================================================\n");
    printf("   ALL INTEGRATION TESTS PASSED SUCCESSFULLY!       \n");
    printf("=====================================================\n");

    return 0;
}
