/**
 * PulseMetrics - Systems Programming Project
 * Module: Collector Service Main Entry Point
 * File: collector/src/main.c
 */

#include "collector.h"
#include "../../core/include/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static PulseCollector *g_collector = NULL;
static HANDLE g_shutdown_event = NULL;
static HANDLE g_collector_mutex = NULL;

static BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        LOG_INFO("Shutdown signal (%lu) received. Terminating safely...", signal);
        if (g_collector) {
            collector_stop(g_collector);
        }
        return TRUE;
    }
    return FALSE;
}

static DWORD WINAPI shutdown_listener_thread(LPVOID param) {
    (void)param;
    if (g_shutdown_event) {
        WaitForSingleObject(g_shutdown_event, INFINITE);
        LOG_INFO("Named shutdown event received. Terminating collector gracefully...");
        if (g_collector) {
            collector_stop(g_collector);
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *config_path = (argc > 1) ? argv[1] : "config/config.json";

    /* Initialize Logger to stdout */
    logger_init(LOG_LEVEL_INFO, stdout);

    printf("=====================================================\n");
    printf("   PulseMetrics - Core StatsD Metrics Collector     \n");
    printf("   (Pure C17 Systems Programming Project)           \n");
    printf("=====================================================\n");

    /* Singleton check: ensure only one collector is running */
    g_collector_mutex = CreateMutexA(NULL, FALSE, "PulseMetrics_Collector_Mutex");
    if (!g_collector_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        LOG_WARN("Another instance of pulsecollector is already active. Exiting.");
        if (g_collector_mutex) CloseHandle(g_collector_mutex);
        return 0;
    }

    /* Create named shutdown event and listener thread */
    g_shutdown_event = CreateEventA(NULL, TRUE, FALSE, "PulseMetrics_Collector_Shutdown");
    HANDLE hThread = CreateThread(NULL, 0, shutdown_listener_thread, NULL, 0, NULL);
    if (hThread) CloseHandle(hThread);

    /* Step 1: Load configuration */
    PulseConfig config;
    config_load_from_file(&config, config_path);
    const char *env_db = getenv("PULSEMETRICS_DB_PATH");
    if (env_db && env_db[0] != '\0') {
        strncpy_s(config.db_path, sizeof(config.db_path), env_db, _TRUNCATE);
    }
    logger_set_level(logger_level_from_string(config.log_level));
    config_print(&config);

    /* Step 2: Create collector instance */
    g_collector = collector_create(&config);
    if (!g_collector) {
        LOG_ERROR("Fatal: Failed to start PulseCollector service.");
        return 1;
    }

    /* Step 3: Register Windows Console control handler for graceful Ctrl+C */
    SetConsoleCtrlHandler(console_handler, TRUE);

    /* Step 4: Run UDP server on main thread */
    collector_run(g_collector);

    /* Step 5: Clean teardown */
    collector_destroy(g_collector);
    g_collector = NULL;

    if (g_shutdown_event) {
        CloseHandle(g_shutdown_event);
        g_shutdown_event = NULL;
    }
    if (g_collector_mutex) {
        CloseHandle(g_collector_mutex);
        g_collector_mutex = NULL;
    }

    LOG_INFO("PulseCollector terminated cleanly. Goodbye!");
    return 0;
}

