/**
 * PulseMetrics - Systems Programming Project
 * Module: Stateful Alerts Engine
 * File: core/src/alerts.c
 */

#include "alerts.h"
#include <stdio.h>
#include <string.h>

void alert_tracker_init(AlertTracker *tracker) {
    if (!tracker) return;
    memset(tracker, 0, sizeof(AlertTracker));
}

static TrackedAlert *get_tracked_entry(AlertTracker *tracker, const char *metric, const char *type) {
    for (size_t i = 0; i < tracker->count; i++) {
        if (strcmp(tracker->tracked[i].metric_name, metric) == 0 &&
            strcmp(tracker->tracked[i].alert_type, type) == 0) {
            return &tracker->tracked[i];
        }
    }

    if (tracker->count < MAX_TRACKED_ALERTS) {
        TrackedAlert *entry = &tracker->tracked[tracker->count++];
        strncpy_s(entry->metric_name, sizeof(entry->metric_name), metric, _TRUNCATE);
        strncpy_s(entry->alert_type, sizeof(entry->alert_type), type, _TRUNCATE);
        entry->is_active = false;
        entry->last_value = 0.0;
        return entry;
    }
    return NULL;
}

static double get_record_field_value(const AggregationRecord *rec, const char *field) {
    if (!rec || !field) return 0.0;
    if (_stricmp(field, "avg") == 0)  return rec->avg;
    if (_stricmp(field, "max") == 0)  return rec->max;
    if (_stricmp(field, "min") == 0)  return rec->min;
    if (_stricmp(field, "sum") == 0)  return rec->sum;
    if (_stricmp(field, "rate") == 0) return rec->rate;
    return rec->avg;
}

static void handle_threshold_check(AlertTracker *tracker,
                                   const char *metric_name,
                                   const char *alert_type,
                                   double threshold,
                                   double actual_val,
                                   bool is_breached,
                                   const char *message_fmt,
                                   const char *timestamp_iso,
                                   AlertEvent *out_alerts,
                                   size_t *alert_count,
                                   size_t max_alerts) {
    if (*alert_count >= max_alerts) return;

    TrackedAlert *state = get_tracked_entry(tracker, metric_name, alert_type);
    if (!state) return;

    if (is_breached) {
        if (!state->is_active) {
            /* State transition: Normal -> Breached (NEW Alert) */
            state->is_active = true;
            state->last_value = actual_val;

            AlertEvent *a = &out_alerts[(*alert_count)++];
            strncpy_s(a->metric_name, sizeof(a->metric_name), metric_name, _TRUNCATE);
            strncpy_s(a->alert_type, sizeof(a->alert_type), alert_type, _TRUNCATE);
            a->threshold = threshold;
            a->actual_value = actual_val;
            snprintf(a->message, sizeof(a->message), message_fmt, actual_val, threshold);
            strncpy_s(a->triggered_at, sizeof(a->triggered_at), timestamp_iso ? timestamp_iso : "", _TRUNCATE);
            a->resolved = 0;
            a->status = ALERT_STATUS_NEW;
        } else {
            /* Still breached -> do NOT duplicate alert in DB */
            state->last_value = actual_val;
        }
    } else {
        if (state->is_active) {
            /* State transition: Breached -> Resolved */
            state->is_active = false;
            state->last_value = actual_val;

            AlertEvent *a = &out_alerts[(*alert_count)++];
            strncpy_s(a->metric_name, sizeof(a->metric_name), metric_name, _TRUNCATE);
            strncpy_s(a->alert_type, sizeof(a->alert_type), alert_type, _TRUNCATE);
            a->threshold = threshold;
            a->actual_value = actual_val;
            snprintf(a->message, sizeof(a->message), "Resolved: metric returned to normal (%.1f <= %.1f)", 
                     actual_val, threshold);
            strncpy_s(a->triggered_at, sizeof(a->triggered_at), timestamp_iso ? timestamp_iso : "", _TRUNCATE);
            a->resolved = 1;
            a->status = ALERT_STATUS_RESOLVED;
        }
    }
}

size_t alerts_evaluate(AlertTracker *tracker,
                       const AggregationRecord *records, 
                       size_t record_count,
                       const PulseConfig *config,
                       const char *timestamp_iso,
                       AlertEvent *out_alerts,
                       size_t max_alerts) {
    if (!tracker || !records || !config || !out_alerts || record_count == 0 || max_alerts == 0) {
        return 0;
    }

    size_t alert_count = 0;

    for (size_t i = 0; i < record_count && alert_count < max_alerts; i++) {
        const AggregationRecord *rec = &records[i];

        /* 1. Global CPU check */
        if (strcmp(rec->name, "system.cpu_usage") == 0 && config->cpu_threshold > 0.0) {
            bool breached = (rec->avg > config->cpu_threshold);
            handle_threshold_check(tracker, rec->name, "high_cpu", config->cpu_threshold, rec->avg,
                                   breached, "High CPU usage: %.1f%% exceeds threshold of %.1f%%",
                                   timestamp_iso, out_alerts, &alert_count, max_alerts);
        }

        /* 2. Global RAM check */
        if (strcmp(rec->name, "system.ram_usage") == 0 && config->ram_threshold > 0.0) {
            bool breached = (rec->avg > config->ram_threshold);
            handle_threshold_check(tracker, rec->name, "high_ram", config->ram_threshold, rec->avg,
                                   breached, "High RAM usage: %.1f%% exceeds threshold of %.1f%%",
                                   timestamp_iso, out_alerts, &alert_count, max_alerts);
        }

        /* 3. Custom rules check */
        for (int r = 0; r < config->custom_alert_count && alert_count < max_alerts; r++) {
            const CustomAlertRule *rule = &config->custom_alerts[r];
            if (strcmp(rec->name, rule->metric) == 0) {
                double val = get_record_field_value(rec, rule->field);
                bool breached = false;
                if (rule->op == ALERT_OP_GREATER && val > rule->threshold_value) {
                    breached = true;
                } else if (rule->op == ALERT_OP_LESS && val < rule->threshold_value) {
                    breached = true;
                }

                char msg_buf[256];
                snprintf(msg_buf, sizeof(msg_buf), "Metric '%s' %s (%%.2f) %s threshold %%.2f",
                         rule->metric, rule->field, (rule->op == ALERT_OP_GREATER ? ">" : "<"));

                handle_threshold_check(tracker, rule->metric, "threshold_exceeded", 
                                       rule->threshold_value, val, breached, msg_buf,
                                       timestamp_iso, out_alerts, &alert_count, max_alerts);
            }
        }
    }

    return alert_count;
}
