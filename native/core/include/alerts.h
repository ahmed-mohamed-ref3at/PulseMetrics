/**
 * PulseMetrics - Systems Programming Project
 * Module: Stateful Alerts Engine
 * File: core/include/alerts.h
 * 
 * Evaluates metric aggregations against threshold rules while tracking state across
 * aggregation windows to prevent duplicate alerts and detect alert resolutions.
 */

#ifndef PULSE_ALERTS_H
#define PULSE_ALERTS_H

#include "aggregator.h"
#include "config.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TRACKED_ALERTS 32

typedef enum {
    ALERT_STATUS_NEW,      /* Threshold breached in this window (insert new alert record) */
    ALERT_STATUS_ACTIVE,   /* Threshold remains breached (do NOT insert duplicate row) */
    ALERT_STATUS_RESOLVED  /* Value returned to normal range (mark existing alert resolved) */
} AlertStatus;

typedef struct {
    char metric_name[PULSE_MAX_NAME_LEN + 1];
    char alert_type[64];
    double threshold;
    double actual_value;
    char message[256];
    char triggered_at[32];
    int resolved;          /* 0 = active, 1 = resolved */
    AlertStatus status;
} AlertEvent;

typedef struct {
    char metric_name[PULSE_MAX_NAME_LEN + 1];
    char alert_type[64];
    bool is_active;
    double last_value;
} TrackedAlert;

typedef struct {
    TrackedAlert tracked[MAX_TRACKED_ALERTS];
    size_t count;
} AlertTracker;

/**
 * Initializes the alert state tracker.
 */
void alert_tracker_init(AlertTracker *tracker);

/**
 * Evaluates aggregation records against config rules and previous alert state.
 * Emits NEW alerts (for database insertion) and RESOLVED alerts (to update resolution status),
 * while suppressing duplicate rows for continuously ACTIVE alerts.
 * Returns count of emitted alert events.
 */
size_t alerts_evaluate(AlertTracker *tracker,
                       const AggregationRecord *records, 
                       size_t record_count,
                       const PulseConfig *config,
                       const char *timestamp_iso,
                       AlertEvent *out_alerts,
                       size_t max_alerts);

#ifdef __cplusplus
}
#endif

#endif /* PULSE_ALERTS_H */
