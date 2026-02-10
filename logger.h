#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>

// Log levels
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_ERROR
} LogLevel;

// Initialize logger with file path (NULL for stdout only)
void logger_init(const char* log_file, LogLevel min_level);

// Log a message
void log_message(LogLevel level, const char* format, ...);

// Close logger
void logger_close(void);

#endif // LOGGER_H
