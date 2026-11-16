/**
 * PulseMetrics - Systems Programming Project
 * Module: Configuration
 * File: core/src/config.c
 */

#include "config.h"
#include "logger.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_set_defaults(PulseConfig *config) {
    if (!config) return;
    memset(config, 0, sizeof(PulseConfig));

    /* Collector defaults */
    strncpy_s(config->collector_host, sizeof(config->collector_host), "127.0.0.1", _TRUNCATE);
    config->collector_port = 8125;
    config->auto_start_collector = true;

    /* Aggregation defaults */
    config->flush_interval_seconds = 10;

    /* Database defaults */
    strncpy_s(config->db_path, sizeof(config->db_path), "database/pulsemetrics.db", _TRUNCATE);

    /* Monitor defaults */
    config->monitor_interval_seconds = 5;
    strncpy_s(config->monitored_processes[0], 64, "chrome.exe", _TRUNCATE);
    strncpy_s(config->monitored_processes[1], 64, "code.exe", _TRUNCATE);
    strncpy_s(config->monitored_processes[2], 64, "explorer.exe", _TRUNCATE);
    config->monitored_process_count = 3;

    /* Alerts defaults */
    config->cpu_threshold = 80.0;
    config->ram_threshold = 85.0;
    config->custom_alert_count = 0;

    /* GUI defaults */
    config->gui_refresh_seconds = 5;
    strncpy_s(config->gui_theme, sizeof(config->gui_theme), "dark", _TRUNCATE);

    /* Logging defaults */
    strncpy_s(config->log_level, sizeof(config->log_level), "info", _TRUNCATE);
}

static char *read_file_to_string(const char *path) {
    FILE *f = NULL;
    errno_t err = fopen_s(&f, path, "rb");
    if (err != 0 || !f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (length <= 0 || length > 10 * 1024 * 1024) { /* 10 MB sanity check */
        fclose(f);
        return NULL;
    }

    char *buffer = (char *)malloc(length + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    size_t read_bytes = fread(buffer, 1, length, f);
    buffer[read_bytes] = '\0';
    fclose(f);
    return buffer;
}

bool config_load_from_file(PulseConfig *config, const char *file_path) {
    if (!config) return false;

    /* Always start with defaults */
    config_set_defaults(config);

    if (!file_path) {
        file_path = "config/config.json";
    }

    char *json_text = read_file_to_string(file_path);
    if (!json_text) {
        LOG_WARN("Could not open config file '%s'. Using defaults.", file_path);
        return false;
    }

    cJSON *root = cJSON_Parse(json_text);
    free(json_text);

    if (!root) {
        LOG_ERROR("Failed to parse JSON config file '%s'. Using defaults.", file_path);
        return false;
    }

    /* 1. Collector */
    cJSON *collector = cJSON_GetObjectItem(root, "collector");
    if (collector && cJSON_IsObject(collector)) {
        cJSON *host = cJSON_GetObjectItem(collector, "host");
        if (host && cJSON_IsString(host) && host->valuestring) {
            strncpy_s(config->collector_host, sizeof(config->collector_host), host->valuestring, _TRUNCATE);
        }
        cJSON *port = cJSON_GetObjectItem(collector, "port");
        if (port && cJSON_IsNumber(port)) {
            config->collector_port = port->valueint;
        }
        cJSON *auto_start = cJSON_GetObjectItem(collector, "auto_start");
        if (auto_start && cJSON_IsBool(auto_start)) {
            config->auto_start_collector = cJSON_IsTrue(auto_start);
        }
    }

    /* 2. Aggregation */
    cJSON *agg = cJSON_GetObjectItem(root, "aggregation");
    if (agg && cJSON_IsObject(agg)) {
        cJSON *flush = cJSON_GetObjectItem(agg, "flush_interval_seconds");
        if (flush && cJSON_IsNumber(flush) && flush->valueint > 0) {
            config->flush_interval_seconds = flush->valueint;
        }
    }

    /* 3. Database */
    cJSON *db = cJSON_GetObjectItem(root, "database");
    if (db && cJSON_IsObject(db)) {
        cJSON *path = cJSON_GetObjectItem(db, "path");
        if (path && cJSON_IsString(path) && path->valuestring) {
            strncpy_s(config->db_path, sizeof(config->db_path), path->valuestring, _TRUNCATE);
        }
    }

    /* 4. Monitoring */
    cJSON *mon = cJSON_GetObjectItem(root, "monitoring");
    if (mon && cJSON_IsObject(mon)) {
        cJSON *interval = cJSON_GetObjectItem(mon, "interval_seconds");
        if (interval && cJSON_IsNumber(interval) && interval->valueint > 0) {
            config->monitor_interval_seconds = interval->valueint;
        }
        cJSON *procs = cJSON_GetObjectItem(mon, "processes");
        if (procs && cJSON_IsArray(procs)) {
            int count = cJSON_GetArraySize(procs);
            if (count > PULSE_MAX_PROCESSES) count = PULSE_MAX_PROCESSES;
            config->monitored_process_count = 0;
            for (int i = 0; i < count; i++) {
                cJSON *p = cJSON_GetArrayItem(procs, i);
                if (p && cJSON_IsString(p) && p->valuestring) {
                    strncpy_s(config->monitored_processes[config->monitored_process_count], 64, p->valuestring, _TRUNCATE);
                    config->monitored_process_count++;
                }
            }
        }
    }

    /* 5. Alerts */
    cJSON *alerts = cJSON_GetObjectItem(root, "alerts");
    if (alerts && cJSON_IsObject(alerts)) {
        cJSON *cpu = cJSON_GetObjectItem(alerts, "cpu_threshold");
        if (cpu && cJSON_IsNumber(cpu)) config->cpu_threshold = cpu->valuedouble;

        cJSON *ram = cJSON_GetObjectItem(alerts, "ram_threshold");
        if (ram && cJSON_IsNumber(ram)) config->ram_threshold = ram->valuedouble;

        cJSON *custom = cJSON_GetObjectItem(alerts, "custom");
        if (custom && cJSON_IsArray(custom)) {
            int count = cJSON_GetArraySize(custom);
            if (count > PULSE_MAX_CUSTOM_ALERTS) count = PULSE_MAX_CUSTOM_ALERTS;
            config->custom_alert_count = 0;
            for (int i = 0; i < count; i++) {
                cJSON *rule_item = cJSON_GetArrayItem(custom, i);
                if (!rule_item || !cJSON_IsObject(rule_item)) continue;

                cJSON *metric = cJSON_GetObjectItem(rule_item, "metric");
                cJSON *field  = cJSON_GetObjectItem(rule_item, "field");
                cJSON *op     = cJSON_GetObjectItem(rule_item, "operator");
                cJSON *val    = cJSON_GetObjectItem(rule_item, "value");

                if (metric && cJSON_IsString(metric) && field && cJSON_IsString(field) && val && cJSON_IsNumber(val)) {
                    CustomAlertRule *r = &config->custom_alerts[config->custom_alert_count];
                    strncpy_s(r->metric, sizeof(r->metric), metric->valuestring, _TRUNCATE);
                    strncpy_s(r->field, sizeof(r->field), field->valuestring, _TRUNCATE);
                    r->op = (op && cJSON_IsString(op) && strcmp(op->valuestring, "<") == 0) ? ALERT_OP_LESS : ALERT_OP_GREATER;
                    r->threshold_value = val->valuedouble;
                    config->custom_alert_count++;
                }
            }
        }
    }

    /* 6. GUI */
    cJSON *gui = cJSON_GetObjectItem(root, "gui");
    if (gui && cJSON_IsObject(gui)) {
        cJSON *refresh = cJSON_GetObjectItem(gui, "refresh_interval_seconds");
        if (refresh && cJSON_IsNumber(refresh) && refresh->valueint > 0) {
            config->gui_refresh_seconds = refresh->valueint;
        }
        cJSON *theme = cJSON_GetObjectItem(gui, "theme");
        if (theme && cJSON_IsString(theme) && theme->valuestring) {
            strncpy_s(config->gui_theme, sizeof(config->gui_theme), theme->valuestring, _TRUNCATE);
        }
    }

    /* 7. Logging */
    cJSON *logging = cJSON_GetObjectItem(root, "logging");
    if (logging && cJSON_IsObject(logging)) {
        cJSON *lvl = cJSON_GetObjectItem(logging, "level");
        if (lvl && cJSON_IsString(lvl) && lvl->valuestring) {
            strncpy_s(config->log_level, sizeof(config->log_level), lvl->valuestring, _TRUNCATE);
        }
    }

    cJSON_Delete(root);
    LOG_INFO("Configuration successfully loaded from '%s'", file_path);
    return true;
}

void config_print(const PulseConfig *config) {
    if (!config) return;
    LOG_INFO("PulseMetrics Configuration:");
    LOG_INFO("  Collector: %s:%d", config->collector_host, config->collector_port);
    LOG_INFO("  Aggregation Flush: %d seconds", config->flush_interval_seconds);
    LOG_INFO("  Database: %s", config->db_path);
    LOG_INFO("  Process Monitor: interval=%ds, %d targets", config->monitor_interval_seconds, config->monitored_process_count);
    LOG_INFO("  Alert Thresholds: CPU=%.1f%%, RAM=%.1f%%, Custom rules=%d",
             config->cpu_threshold, config->ram_threshold, config->custom_alert_count);
}

