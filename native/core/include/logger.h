/**
 * PulseMetrics - Systems Programming Project
 * Module: Logger
 * File: core/include/logger.h
 * 
 * Provides thread-safe, timestamped logging utilities with configurable log levels.
 */

#ifndef PULSE_LOGGER_H
#define PULSE_LOGGER_H

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3,
    LOG_LEVEL_NONE  = 4
} LogLevel;

/**
 * Initializes the logger with an output stream (e.g. stdout or stderr) and initial log level.
 */
void logger_init(LogLevel level, FILE *stream);

/**
 * Sets the active logging threshold.
 */
void logger_set_level(LogLevel level);

/**
 * Converts a string (e.g., "debug", "info", "warn", "error") to LogLevel.
 */
LogLevel logger_level_from_string(const char *str);

/**
 * Core logging dispatch function. Called directly or via helper macros.
 */
void logger_log(LogLevel level, const char *file, int line, const char *fmt, ...);

/* Helper macros passing file and line context */
#define LOG_DEBUG(...) logger_log(LOG_LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  logger_log(LOG_LEVEL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  logger_log(LOG_LEVEL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) logger_log(LOG_LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* PULSE_LOGGER_H */

