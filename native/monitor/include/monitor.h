/**
 * PulseMetrics - Systems Programming Project
 * Module: Windows API Process & System Monitor
 * File: monitor/include/monitor.h
 */

#ifndef PULSE_MONITOR_H
#define PULSE_MONITOR_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <stdbool.h>
#include <stdint.h>

#include "../../core/include/config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double cpu_usage_percent;
    double ram_usage_percent;
    double ram_used_mb;
    double ram_total_mb;
} SystemMetrics;

typedef struct {
    char name[64];
    uint32_t instance_count; /* Number of running instances sharing this name */
    double cpu_percent;      /* Summed CPU utilization across all instances */
    double ram_mb;           /* Summed Working Set RAM (MB) across all instances */
} ProcessMetrics;

/**
 * Initializes the monitoring subsystem (records initial CPU clock baseline).
 */
void monitor_init(void);

/**
 * Samples current system-wide CPU and RAM utilization using native Windows APIs.
 */
bool monitor_get_system_metrics(SystemMetrics *out_metrics);

/**
 * Samples and aggregates resource utilization for configured target process names.
 * Multiple processes with the same name are summed into one entry.
 * Unreachable processes (Access Denied) are safely skipped.
 * Returns count of aggregated process targets.
 */
size_t monitor_get_process_metrics(const char monitored_names[][64], 
                                  size_t monitored_count,
                                  ProcessMetrics *out_processes, 
                                  size_t max_processes);

/**
 * Sanitizes a process name for StatsD metric naming (e.g. "Chrome.exe" -> "chrome_exe").
 */
void monitor_sanitize_metric_name(const char *raw_name, char *out_name, size_t out_len);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_MONITOR_H */
