#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

static FILE* log_fp = NULL;
static LogLevel min_log_level = LOG_INFO;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static const char* level_strings[] = {
    "DEBUG",
    "INFO",
    "ERROR"
};

void logger_init(const char* log_file, LogLevel min_level) {
    pthread_mutex_lock(&log_mutex);
    
    min_log_level = min_level;
    
    if (log_file != NULL) {
        log_fp = fopen(log_file, "a");
        if (log_fp == NULL) {
            fprintf(stderr, "Failed to open log file: %s\n", log_file);
            log_fp = NULL;
        }
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void log_message(LogLevel level, const char* format, ...) {
    if (level < min_log_level) {
        return;
    }
    
    pthread_mutex_lock(&log_mutex);
    
    // Get current timestamp
    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Print to stdout
    printf("[%s] [%s] ", timestamp, level_strings[level]);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
    fflush(stdout);
    
    // Print to file if available
    if (log_fp != NULL) {
        fprintf(log_fp, "[%s] [%s] ", timestamp, level_strings[level]);
        va_start(args, format);
        vfprintf(log_fp, format, args);
        va_end(args);
        fprintf(log_fp, "\n");
        fflush(log_fp);
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void logger_close(void) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_fp != NULL) {
        fclose(log_fp);
        log_fp = NULL;
    }
    
    pthread_mutex_unlock(&log_mutex);
}
