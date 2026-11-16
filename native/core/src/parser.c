/**
 * PulseMetrics - Systems Programming Project
 * Module: Parser & Validator
 * File: core/src/parser.c
 */

#include "parser.h"
#include "logger.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

const char *parser_result_to_string(ParseResult res) {
    switch (res) {
        case PARSE_OK:                  return "OK";
        case PARSE_ERR_NULL_OR_EMPTY:    return "Empty or null input";
        case PARSE_ERR_DELIMITER_MISSING: return "Missing ':' or '|' delimiter";
        case PARSE_ERR_NAME_INVALID:      return "Invalid metric name characters or does not start with letter";
        case PARSE_ERR_NAME_TOO_LONG:     return "Metric name exceeds maximum length";
        case PARSE_ERR_VALUE_INVALID:     return "Value is not a valid floating-point number";
        case PARSE_ERR_TYPE_UNKNOWN:      return "Unknown metric type (expected 'c', 'g', or 'ms')";
        case PARSE_ERR_NEGATIVE_VALUE:    return "Negative value not allowed for counter or timer";
        default:                         return "Unknown error";
    }
}

const char *metric_type_to_string(MetricType type) {
    switch (type) {
        case METRIC_TYPE_COUNTER: return "counter";
        case METRIC_TYPE_GAUGE:   return "gauge";
        case METRIC_TYPE_TIMER:   return "timer";
        default:                  return "unknown";
    }
}

static bool is_valid_name_char(char c) {
    return isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-';
}

ParseResult parser_parse_line(const char *line, ParsedMetric *out_metric) {
    if (!line || !out_metric) {
        return PARSE_ERR_NULL_OR_EMPTY;
    }

    /* Skip leading whitespace */
    while (*line && isspace((unsigned char)*line)) {
        line++;
    }

    if (*line == '\0') {
        return PARSE_ERR_NULL_OR_EMPTY;
    }

    /* 1. Locate ':' delimiter between name and value */
    const char *colon = strchr(line, ':');
    if (!colon) {
        return PARSE_ERR_DELIMITER_MISSING;
    }

    size_t name_len = colon - line;
    if (name_len == 0) {
        return PARSE_ERR_NAME_INVALID;
    }
    if (name_len > PULSE_MAX_NAME_LEN) {
        return PARSE_ERR_NAME_TOO_LONG;
    }

    /* Metric name must start with an ASCII letter */
    if (!isalpha((unsigned char)line[0])) {
        return PARSE_ERR_NAME_INVALID;
    }

    /* Check each character in the name */
    for (size_t i = 0; i < name_len; i++) {
        if (!is_valid_name_char(line[i])) {
            return PARSE_ERR_NAME_INVALID;
        }
    }

    /* 2. Locate '|' delimiter between value and type */
    const char *pipe = strchr(colon + 1, '|');
    if (!pipe) {
        return PARSE_ERR_DELIMITER_MISSING;
    }

    /* Parse numeric value */
    const char *val_str = colon + 1;
    char *endptr = NULL;
    double val = strtod(val_str, &endptr);

    /* Ensure strtod consumed up to the pipe character (ignoring any intermediate spaces) */
    if (endptr == val_str) {
        return PARSE_ERR_VALUE_INVALID;
    }
    while (endptr < pipe && isspace((unsigned char)*endptr)) {
        endptr++;
    }
    if (endptr != pipe) {
        return PARSE_ERR_VALUE_INVALID;
    }

    /* 3. Parse type identifier */
    const char *type_str = pipe + 1;
    /* Trim trailing spaces / carriage returns */
    char type_buf[16];
    size_t t_idx = 0;
    while (*type_str && !isspace((unsigned char)*type_str) && t_idx < sizeof(type_buf) - 1) {
        type_buf[t_idx++] = *type_str++;
    }
    type_buf[t_idx] = '\0';

    MetricType m_type;
    if (strcmp(type_buf, "c") == 0) {
        m_type = METRIC_TYPE_COUNTER;
        if (val < 0.0) {
            return PARSE_ERR_NEGATIVE_VALUE;
        }
    } else if (strcmp(type_buf, "g") == 0) {
        m_type = METRIC_TYPE_GAUGE;
    } else if (strcmp(type_buf, "ms") == 0) {
        m_type = METRIC_TYPE_TIMER;
        if (val < 0.0) {
            return PARSE_ERR_NEGATIVE_VALUE;
        }
    } else {
        return PARSE_ERR_TYPE_UNKNOWN;
    }

    /* Successfully populate output */
    memcpy(out_metric->name, line, name_len);
    out_metric->name[name_len] = '\0';
    out_metric->value = val;
    out_metric->type = m_type;

    return PARSE_OK;
}

size_t parser_parse_buffer(const char *buffer, size_t buffer_len, MetricLineCallback callback, void *user_data) {
    if (!buffer || buffer_len == 0 || !callback) {
        return 0;
    }

    size_t valid_count = 0;
    const char *cursor = buffer;
    const char *end = buffer + buffer_len;

    while (cursor < end) {
        /* Find next newline or end of buffer */
        const char *nl = (const char *)memchr(cursor, '\n', end - cursor);
        size_t line_len = nl ? (size_t)(nl - cursor) : (size_t)(end - cursor);

        if (line_len > 0) {
            /* Copy line into temp buffer and null-terminate */
            char temp_line[512];
            size_t copy_len = (line_len < sizeof(temp_line) - 1) ? line_len : sizeof(temp_line) - 1;
            memcpy(temp_line, cursor, copy_len);
            temp_line[copy_len] = '\0';

            /* Strip trailing carriage return if present */
            if (copy_len > 0 && temp_line[copy_len - 1] == '\r') {
                temp_line[copy_len - 1] = '\0';
            }

            ParsedMetric metric;
            ParseResult res = parser_parse_line(temp_line, &metric);
            if (res == PARSE_OK) {
                callback(&metric, user_data);
                valid_count++;
            } else if (res != PARSE_ERR_NULL_OR_EMPTY) {
                LOG_WARN("Metric parse error [%s] for line: '%.32s...'", parser_result_to_string(res), temp_line);
            }
        }

        if (!nl) break;
        cursor = nl + 1;
    }

    return valid_count;
}

