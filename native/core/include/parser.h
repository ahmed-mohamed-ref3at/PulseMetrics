/**
 * PulseMetrics - Systems Programming Project
 * Module: Parser & Validator
 * File: core/include/parser.h
 * 
 * Validates and extracts StatsD-format metric messages.
 */

#ifndef PULSE_PARSER_H
#define PULSE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    METRIC_TYPE_COUNTER,
    METRIC_TYPE_GAUGE,
    METRIC_TYPE_TIMER,
    METRIC_TYPE_UNKNOWN
} MetricType;

typedef enum {
    PARSE_OK = 0,
    PARSE_ERR_NULL_OR_EMPTY,
    PARSE_ERR_DELIMITER_MISSING,
    PARSE_ERR_NAME_INVALID,
    PARSE_ERR_NAME_TOO_LONG,
    PARSE_ERR_VALUE_INVALID,
    PARSE_ERR_TYPE_UNKNOWN,
    PARSE_ERR_NEGATIVE_VALUE
} ParseResult;

typedef struct {
    char name[PULSE_MAX_NAME_LEN + 1];
    double value;
    MetricType type;
} ParsedMetric;

/**
 * Returns human-readable error description for a ParseResult code.
 */
const char *parser_result_to_string(ParseResult res);

/**
 * Converts MetricType enum to string representation ("counter", "gauge", "timer").
 */
const char *metric_type_to_string(MetricType type);

/**
 * Parses a single null-terminated StatsD line: "<metric_name>:<value>|<type>"
 * Safe string parsing; does not modify input string.
 */
ParseResult parser_parse_line(const char *line, ParsedMetric *out_metric);

/**
 * Callback signature for batch message processing.
 */
typedef void (*MetricLineCallback)(const ParsedMetric *metric, void *user_data);

/**
 * Parses a raw UDP buffer containing one or more newline-separated metric lines.
 * Invalid lines are safely logged and skipped; valid lines trigger callback.
 * Returns total count of successfully parsed metrics.
 */
size_t parser_parse_buffer(const char *buffer, size_t buffer_len, MetricLineCallback callback, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_PARSER_H */

