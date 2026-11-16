/**
 * PulseMetrics - Systems Programming Project
 * Module: Windows API Process & System Monitor (Standalone Executable)
 * File: monitor/src/main.c
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "monitor.h"
#include "../../core/include/logger.h"
#include "../../core/include/config.h"

static volatile bool s_running = true;
static HANDLE g_monitor_shutdown = NULL;
static HANDLE g_monitor_mutex = NULL;

static BOOL WINAPI console_handler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT || signal == CTRL_BREAK_EVENT) {
        LOG_INFO("Shutdown signal (%lu) received. Stopping monitor...", signal);
        s_running = false;
        return TRUE;
    }
    return FALSE;
}

static DWORD WINAPI shutdown_listener_thread(LPVOID param) {
    (void)param;
    if (g_monitor_shutdown) {
        WaitForSingleObject(g_monitor_shutdown, INFINITE);
        LOG_INFO("Named shutdown event received. Stopping monitor gracefully...");
        s_running = false;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *config_path = (argc > 1) ? argv[1] : "config/config.json";

    logger_init(LOG_LEVEL_INFO, stdout);

    printf("=====================================================\n");
    printf("   PulseMetrics - Windows Native Process Monitor    \n");
    printf("   (Pure C17 / Win32 API / UDP Client)              \n");
    printf("=====================================================\n");

    /* Singleton check: ensure only one monitor instance is running */
    g_monitor_mutex = CreateMutexA(NULL, FALSE, "PulseMetrics_Monitor_Mutex");
    if (!g_monitor_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        LOG_WARN("Another instance of pulsemonitor is already active. Exiting.");
        if (g_monitor_mutex) CloseHandle(g_monitor_mutex);
        return 0;
    }

    /* Named shutdown event and listener thread */
    g_monitor_shutdown = CreateEventA(NULL, TRUE, FALSE, "PulseMetrics_Monitor_Shutdown");
    HANDLE hThread = CreateThread(NULL, 0, shutdown_listener_thread, NULL, 0, NULL);
    if (hThread) CloseHandle(hThread);

    /* 1. Load configuration */
    PulseConfig config;
    config_load_from_file(&config, config_path);

    /* 2. Setup Winsock client socket */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        LOG_ERROR("WSAStartup failed in monitor: %d", WSAGetLastError());
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        LOG_ERROR("Failed to create UDP socket: %d", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    struct sockaddr_in target_addr;
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons((u_short)config.collector_port);
    target_addr.sin_addr.s_addr = inet_addr(config.collector_host);
    if (target_addr.sin_addr.s_addr == INADDR_NONE) {
        inet_pton(AF_INET, config.collector_host, &target_addr.sin_addr);
    }

    /* 3. Initialize Windows monitoring subsystem */
    monitor_init();
    SetConsoleCtrlHandler(console_handler, TRUE);

    LOG_INFO("Monitor active -> Sending system metrics to %s:%d every %ds",
             config.collector_host, config.collector_port, config.monitor_interval_seconds);

    char send_buf[1024];
    ProcessMetrics proc_list[PULSE_MAX_PROCESSES];

    while (s_running) {
        /* Sample System-wide CPU and RAM */
        SystemMetrics sys;
        if (monitor_get_system_metrics(&sys)) {
            int len = snprintf(send_buf, sizeof(send_buf),
                               "system.cpu_usage:%.1f|g\n"
                               "system.ram_usage:%.1f|g\n"
                               "system.ram_used_mb:%.1f|g\n"
                               "system.ram_total_mb:%.1f|g",
                               sys.cpu_usage_percent,
                               sys.ram_usage_percent,
                               sys.ram_used_mb,
                               sys.ram_total_mb);

            if (len > 0) {
                sendto(sock, send_buf, len, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
            }

            LOG_INFO("Sent system stats: CPU=%.1f%%, RAM=%.1f%% (%.0f / %.0f MB)",
                     sys.cpu_usage_percent, sys.ram_usage_percent, sys.ram_used_mb, sys.ram_total_mb);
        }

        /* Sample target processes */
        size_t p_count = monitor_get_process_metrics((const char (*)[64])config.monitored_processes,
                                                    (size_t)config.monitored_process_count,
                                                    proc_list, PULSE_MAX_PROCESSES);

        for (size_t p = 0; p < p_count; p++) {
            char clean_name[64];
            monitor_sanitize_metric_name(proc_list[p].name, clean_name, sizeof(clean_name));

            /* Send summed RAM, summed CPU, and process instance count */
            int len = snprintf(send_buf, sizeof(send_buf), 
                               "process.%s.ram_mb:%.1f|g\n"
                               "process.%s.cpu:%.1f|g\n"
                               "process.%s.count:%u|g",
                               clean_name, proc_list[p].ram_mb,
                               clean_name, proc_list[p].cpu_percent,
                               clean_name, proc_list[p].instance_count);

            if (len > 0) {
                sendto(sock, send_buf, len, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
            }
        }

        if (p_count > 0) {
            LOG_INFO("Sent aggregated metrics for %zu process groups", p_count);
        }

        /* Sleep in 100ms intervals to handle prompt Ctrl+C interrupt */
        int sleep_chunks = config.monitor_interval_seconds * 10;
        for (int i = 0; i < sleep_chunks && s_running; i++) {
            Sleep(100);
        }
    }

    LOG_INFO("Closing monitor socket and exiting...");
    closesocket(sock);
    WSACleanup();

    if (g_monitor_shutdown) {
        CloseHandle(g_monitor_shutdown);
        g_monitor_shutdown = NULL;
    }
    if (g_monitor_mutex) {
        CloseHandle(g_monitor_mutex);
        g_monitor_mutex = NULL;
    }

    return 0;
}

