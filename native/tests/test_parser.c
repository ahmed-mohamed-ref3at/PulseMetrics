/**
 * PulseMetrics - Systems Programming Project
 * Unit Test: Wire Protocol Parser
 * File: tests/test_parser.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "parser.h"

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "ASSERTION FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        exit(1); \
    } \
} while(0)

static void test_parse_valid_counter(void) {
    ParsedMetric m;
    ParseResult res = parser_parse_line("app.requests:5|c", &m);
    TEST_ASSERT(res == PARSE_OK);
    TEST_ASSERT(strcmp(m.name, "app.requests") == 0);
    TEST_ASSERT(fabs(m.value - 5.0) < 1e-6);
    TEST_ASSERT(m.type == METRIC_TYPE_COUNTER);
}

static void test_parse_valid_gauge(void) {
    ParsedMetric m;
    ParseResult res = parser_parse_line("system.memory.used:1024.5|g", &m);
    TEST_ASSERT(res == PARSE_OK);
    TEST_ASSERT(strcmp(m.name, "system.memory.used") == 0);
    TEST_ASSERT(fabs(m.value - 1024.5) < 1e-6);
    TEST_ASSERT(m.type == METRIC_TYPE_GAUGE);
}

static void test_parse_valid_timer(void) {
    ParsedMetric m;
    ParseResult res = parser_parse_line("db.query_time:42.8|ms", &m);
    TEST_ASSERT(res == PARSE_OK);
    TEST_ASSERT(strcmp(m.name, "db.query_time") == 0);
    TEST_ASSERT(fabs(m.value - 42.8) < 1e-6);
    TEST_ASSERT(m.type == METRIC_TYPE_TIMER);
}

typedef struct {
    ParsedMetric metrics[10];
    size_t count;
} BatchContext;

static void batch_cb(const ParsedMetric *m, void *user_data) {
    BatchContext *ctx = (BatchContext *)user_data;
    if (ctx->count < 10) {
        ctx->metrics[ctx->count++] = *m;
    }
}

static void test_parse_batch(void) {
    const char *payload = "m.counter:1|c\nm.gauge:25|g\nm.timer:150|ms\n";
    BatchContext ctx = {0};
    size_t count = parser_parse_buffer(payload, strlen(payload), batch_cb, &ctx);
    TEST_ASSERT(count == 3);
    TEST_ASSERT(ctx.count == 3);

    TEST_ASSERT(strcmp(ctx.metrics[0].name, "m.counter") == 0);
    TEST_ASSERT(ctx.metrics[0].type == METRIC_TYPE_COUNTER);
    TEST_ASSERT(fabs(ctx.metrics[0].value - 1.0) < 1e-6);

    TEST_ASSERT(strcmp(ctx.metrics[1].name, "m.gauge") == 0);
    TEST_ASSERT(ctx.metrics[1].type == METRIC_TYPE_GAUGE);
    TEST_ASSERT(fabs(ctx.metrics[1].value - 25.0) < 1e-6);

    TEST_ASSERT(strcmp(ctx.metrics[2].name, "m.timer") == 0);
    TEST_ASSERT(ctx.metrics[2].type == METRIC_TYPE_TIMER);
    TEST_ASSERT(fabs(ctx.metrics[2].value - 150.0) < 1e-6);
}

static void test_parse_invalid(void) {
    ParsedMetric m;

    /* Missing colon */
    TEST_ASSERT(parser_parse_line("invalid_no_colon", &m) != PARSE_OK);

    /* Missing pipe */
    TEST_ASSERT(parser_parse_line("name:100", &m) != PARSE_OK);

    /* Non-numeric value */
    TEST_ASSERT(parser_parse_line("name:abc|c", &m) != PARSE_OK);

    /* Unknown metric type */
    TEST_ASSERT(parser_parse_line("name:100|unknown", &m) != PARSE_OK);

    /* Empty line */
    TEST_ASSERT(parser_parse_line("", &m) != PARSE_OK);
}

int main(void) {
    printf("[TEST] Running test_parser...\n");
    test_parse_valid_counter();
    test_parse_valid_gauge();
    test_parse_valid_timer();
    test_parse_batch();
    test_parse_invalid();
    printf("[PASS] test_parser completed successfully!\n");
    return 0;
}
