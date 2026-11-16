/**
 * PulseMetrics - Systems Programming Project
 * Module: Windows API Process & System Monitor
 * File: monitor/src/monitor.c
 */

#include "monitor.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX_PID_TRACKERS 1024

typedef struct {
    DWORD pid;
    uint64_t prev_proc_time;
    uint64_t prev_sys_time;
    bool active;
} PidSnapshot;

static PidSnapshot s_pid_cache[MAX_PID_TRACKERS];
static size_t s_pid_cache_count = 0;

static uint64_t s_prev_idle = 0;
static uint64_t s_prev_kernel = 0;
static uint64_t s_prev_user = 0;
static bool s_has_prev_cpu = false;

static uint64_t filetime_to_uint64(const FILETIME *ft) {
    ULARGE_INTEGER li;
    li.LowPart = ft->dwLowDateTime;
    li.HighPart = ft->dwHighDateTime;
    return li.QuadPart;
}

void monitor_init(void) {
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        s_prev_idle = filetime_to_uint64(&idle);
        s_prev_kernel = filetime_to_uint64(&kernel);
        s_prev_user = filetime_to_uint64(&user);
        s_has_prev_cpu = true;
    }
    s_pid_cache_count = 0;
}

bool monitor_get_system_metrics(SystemMetrics *out) {
    if (!out) return false;

    /* 1. Calculate CPU usage percentage via GetSystemTimes delta */
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        uint64_t curr_idle = filetime_to_uint64(&idle);
        uint64_t curr_kernel = filetime_to_uint64(&kernel);
        uint64_t curr_user = filetime_to_uint64(&user);

        if (s_has_prev_cpu) {
            uint64_t idle_diff = curr_idle - s_prev_idle;
            uint64_t kernel_diff = curr_kernel - s_prev_kernel;
            uint64_t user_diff = curr_user - s_prev_user;
            uint64_t total_diff = kernel_diff + user_diff;

            if (total_diff > 0) {
                out->cpu_usage_percent = 100.0 * (1.0 - ((double)idle_diff / (double)total_diff));
                if (out->cpu_usage_percent < 0.0) out->cpu_usage_percent = 0.0;
                if (out->cpu_usage_percent > 100.0) out->cpu_usage_percent = 100.0;
            } else {
                out->cpu_usage_percent = 0.0;
            }
        } else {
            out->cpu_usage_percent = 0.0;
        }

        s_prev_idle = curr_idle;
        s_prev_kernel = curr_kernel;
        s_prev_user = curr_user;
        s_has_prev_cpu = true;
    } else {
        out->cpu_usage_percent = 0.0;
    }

    /* 2. Retrieve System RAM metrics */
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        out->ram_usage_percent = (double)mem.dwMemoryLoad;
        out->ram_total_mb = (double)mem.ullTotalPhys / (1024.0 * 1024.0);
        out->ram_used_mb = (double)(mem.ullTotalPhys - mem.ullAvailPhys) / (1024.0 * 1024.0);
    } else {
        out->ram_usage_percent = 0.0;
        out->ram_total_mb = 0.0;
        out->ram_used_mb = 0.0;
    }

    return true;
}

void monitor_sanitize_metric_name(const char *raw, char *out, size_t out_len) {
    if (!raw || !out || out_len == 0) return;

    size_t i = 0;
    while (*raw && i < out_len - 1) {
        char c = *raw++;
        if (isalnum((unsigned char)c) || c == '-' || c == '_') {
            out[i++] = (char)tolower((unsigned char)c);
        } else if (c == ' ' || c == '.') {
            out[i++] = '_';
        }
    }
    out[i] = '\0';
}

static double calculate_pid_cpu(DWORD pid, HANDLE hProc, uint64_t curr_sys_time) {
    FILETIME creation_time, exit_time, kernel_time, user_time;
    if (!GetProcessTimes(hProc, &creation_time, &exit_time, &kernel_time, &user_time)) {
        return 0.0;
    }

    uint64_t curr_proc_time = filetime_to_uint64(&kernel_time) + filetime_to_uint64(&user_time);

    /* Search for PID in cache */
    for (size_t i = 0; i < s_pid_cache_count; i++) {
        if (s_pid_cache[i].pid == pid) {
            s_pid_cache[i].active = true;
            uint64_t proc_delta = curr_proc_time - s_pid_cache[i].prev_proc_time;
            uint64_t sys_delta = curr_sys_time - s_pid_cache[i].prev_sys_time;

            s_pid_cache[i].prev_proc_time = curr_proc_time;
            s_pid_cache[i].prev_sys_time = curr_sys_time;

            if (sys_delta > 0) {
                double pct = 100.0 * ((double)proc_delta / (double)sys_delta);
                return (pct >= 0.0) ? pct : 0.0;
            }
            return 0.0;
        }
    }

    /* Not in cache: register new PID entry */
    if (s_pid_cache_count < MAX_PID_TRACKERS) {
        s_pid_cache[s_pid_cache_count].pid = pid;
        s_pid_cache[s_pid_cache_count].prev_proc_time = curr_proc_time;
        s_pid_cache[s_pid_cache_count].prev_sys_time = curr_sys_time;
        s_pid_cache[s_pid_cache_count].active = true;
        s_pid_cache_count++;
    }
    return 0.0;
}

size_t monitor_get_process_metrics(const char monitored_names[][64], 
                                  size_t monitored_count,
                                  ProcessMetrics *out_processes, 
                                  size_t max_processes) {
    if (!monitored_names || monitored_count == 0 || !out_processes || max_processes == 0) {
        return 0;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    /* Get current system CPU time baseline */
    FILETIME sys_idle, sys_kernel, sys_user;
    uint64_t curr_sys_time = 0;
    if (GetSystemTimes(&sys_idle, &sys_kernel, &sys_user)) {
        curr_sys_time = filetime_to_uint64(&sys_kernel) + filetime_to_uint64(&sys_user);
    }

    /* Reset active flag in PID cache */
    for (size_t i = 0; i < s_pid_cache_count; i++) {
        s_pid_cache[i].active = false;
    }

    /* Initialize output array to zero */
    memset(out_processes, 0, max_processes * sizeof(ProcessMetrics));
    size_t result_count = 0;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(snapshot, &pe)) {
        do {
            for (size_t i = 0; i < monitored_count; i++) {
                if (_stricmp(pe.szExeFile, monitored_names[i]) == 0) {
                    /* Open process safely; skip if access denied without crashing */
                    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, 
                                               FALSE, pe.th32ProcessID);
                    if (!hProc) {
                        /* Access denied or process terminated, skip gracefully */
                        continue;
                    }

                    PROCESS_MEMORY_COUNTERS pmc;
                    double ram_mb = 0.0;
                    if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                        ram_mb = (double)pmc.WorkingSetSize / (1024.0 * 1024.0);
                    }

                    double cpu_pct = calculate_pid_cpu(pe.th32ProcessID, hProc, curr_sys_time);
                    CloseHandle(hProc);

                    /* Group and sum into aggregated entry by process name */
                    int found_idx = -1;
                    for (size_t g = 0; g < result_count; g++) {
                        if (_stricmp(out_processes[g].name, pe.szExeFile) == 0) {
                            found_idx = (int)g;
                            break;
                        }
                    }

                    if (found_idx >= 0) {
                        out_processes[found_idx].instance_count++;
                        out_processes[found_idx].ram_mb += ram_mb;
                        out_processes[found_idx].cpu_percent += cpu_pct;
                    } else if (result_count < max_processes) {
                        strncpy_s(out_processes[result_count].name, sizeof(out_processes[result_count].name), 
                                  pe.szExeFile, _TRUNCATE);
                        out_processes[result_count].instance_count = 1;
                        out_processes[result_count].ram_mb = ram_mb;
                        out_processes[result_count].cpu_percent = cpu_pct;
                        result_count++;
                    }
                    break;
                }
            }
        } while (Process32Next(snapshot, &pe));
    }

    CloseHandle(snapshot);

    /* Purge dead PIDs from cache */
    size_t write_idx = 0;
    for (size_t i = 0; i < s_pid_cache_count; i++) {
        if (s_pid_cache[i].active) {
            s_pid_cache[write_idx++] = s_pid_cache[i];
        }
    }
    s_pid_cache_count = write_idx;

    return result_count;
}
