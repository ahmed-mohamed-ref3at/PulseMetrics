/**
 * PulseMetrics - Systems Programming Project
 * Module: Configuration
 * File: core/include/config.h
 * 
 * Provides configuration data structures and JSON loader with robust defaults.
 */

#ifndef PULSE_CONFIG_H
#define PULSE_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PULSE_MAX_PATH_LEN       260
#define PULSE_MAX_NAME_LEN       128
#define PULSE_MAX_HOST_LEN       64
#define PULSE_MAX_PROCESSES      32
#define PULSE_MAX_CUSTOM_ALERTS  16

typedef enum {
    ALERT_OP_GREATER = 0,
    ALERT_OP_LESS    = 1
} AlertOperator;

typedef struct {
    char metric[PULSE_MAX_NAME_LEN];
    char field[16];          /* "avg", "max", "min", "sum", "rate" */
    AlertOperator op;        /* > or < */
    double threshold_value;
} CustomAlertRule;

typedef struct {
    /* Collector Settings */
    char collector_host[PULSE_MAX_HOST_LEN];
    int collector_port;
    bool auto_start_collector;

    /* Aggregation Settings */
    int flush_interval_seconds;

    /* Database Settings */
    char db_path[PULSE_MAX_PATH_LEN];

    /* System Process Monitor Settings */
    int monitor_interval_seconds;
    char monitored_processes[PULSE_MAX_PROCESSES][64];
    int monitored_process_count;

    /* Alert Thresholds */
    double cpu_threshold;
    double ram_threshold;
    CustomAlertRule custom_alerts[PULSE_MAX_CUSTOM_ALERTS];
    int custom_alert_count;

    /* GUI Display Settings */
    int gui_refresh_seconds;
    char gui_theme[32];

    /* Logging */
    char log_level[32];
} PulseConfig;

/**
 * Initializes a configuration struct with safe, recommended defaults.
 */
void config_set_defaults(PulseConfig *config);

/**
 * Loads configuration from a JSON file path. Missing fields will retain default values.
 * Returns true on success, false if file could not be read (defaults still applied).
 */
bool config_load_from_file(PulseConfig *config, const char *file_path);

/**
 * Prints configuration details to log.
 */
void config_print(const PulseConfig *config);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_CONFIG_H */

