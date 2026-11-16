/**
 * PulseMetrics - Systems Programming Project
 * Unit Test: Read-Only Data Access Layer
 * File: tests/test_data_access.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "data_access.h"
#include "database.h"

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

static void test_dal_nonexistent_db(void) {
    /* Attempting to open non-existent DB must fail cleanly and return NULL */
    DataAccess *dal = dal_open("non_existent_path_xyz123.db");
    TEST_ASSERT(dal == NULL);
}

static void test_dal_read_pipeline(void) {
    const char *test_db_path = "test_dal.db";
    remove(test_db_path);

    /* 1. Setup sample database using core database module */
    DatabaseWriter *writer = db_writer_open(test_db_path);
    TEST_ASSERT(writer != NULL);

    /* Write 1 sample metric */
    int64_t m_id = db_writer_get_or_create_metric(writer, "test.counter", METRIC_TYPE_COUNTER);
    TEST_ASSERT(m_id > 0);

    /* Write 1 aggregation record */
    AggregationRecord rec;
    memset(&rec, 0, sizeof(rec));
    strncpy_s(rec.name, sizeof(rec.name), "test.counter", _TRUNCATE);
    rec.type = METRIC_TYPE_COUNTER;
    rec.sum = 42.0;
    rec.avg = 21.0;
    rec.min = 0.0;
    rec.max = 42.0;
    rec.count = 2;
    rec.rate = 4.2;
    strncpy_s(rec.window_start, sizeof(rec.window_start), "2026-11-12T00:00:00Z", _TRUNCATE);
    strncpy_s(rec.window_end, sizeof(rec.window_end), "2026-11-12T00:00:10Z", _TRUNCATE);

    db_writer_begin_transaction(writer);
    db_writer_insert_aggregation(writer, m_id, &rec);
    db_writer_commit_transaction(writer);
    db_writer_close(writer);

    /* 2. Open via Read-Only Data Access Layer */
    DataAccess *dal = dal_open(test_db_path);
    TEST_ASSERT(dal != NULL);

    /* Check system overview */
    SystemOverview ov;
    bool ok_ov = dal_get_system_overview(dal, &ov);
    TEST_ASSERT(ok_ov);
    TEST_ASSERT(ov.total_metrics == 1);
    TEST_ASSERT(ov.total_aggregations == 1);

    /* Check metrics list */
    MetricInfo *metrics = NULL;
    size_t m_count = 0;
    bool ok_m = dal_get_metrics(dal, &metrics, &m_count);
    TEST_ASSERT(ok_m);
    TEST_ASSERT(m_count == 1);
    TEST_ASSERT(strcmp(metrics[0].name, "test.counter") == 0);
    dal_free_metrics(metrics);

    /* Check latest aggregations */
    AggregationData *aggs = NULL;
    size_t a_count = 0;
    bool ok_a = dal_get_latest_aggregations(dal, "test.counter", 10, &aggs, &a_count);
    TEST_ASSERT(ok_a);
    TEST_ASSERT(a_count == 1);
    TEST_ASSERT(aggs[0].sum == 42.0);
    TEST_ASSERT(aggs[0].count == 2);
    dal_free_aggregations(aggs);

    dal_close(dal);
    remove(test_db_path);
}

int main(void) {
    printf("[TEST] Running test_data_access...\n");
    test_dal_nonexistent_db();
    test_dal_read_pipeline();
    printf("[PASS] test_data_access completed successfully!\n");
    return 0;
}
