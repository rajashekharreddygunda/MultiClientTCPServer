#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_set_defaults(ServerConfig* config) {
    config->port = 8080;
    config->thread_pool_size = 4;
    config->max_connections = 100;
    config->log_level = LOG_INFO;
    strcpy(config->log_file, "");
}

static LogLevel parse_log_level(const char* level_str) {
    if (strcmp(level_str, "DEBUG") == 0) {
        return LOG_DEBUG;
    } else if (strcmp(level_str, "INFO") == 0) {
        return LOG_INFO;
    } else if (strcmp(level_str, "ERROR") == 0) {
        return LOG_ERROR;
    }
    return LOG_INFO;
}

int config_load(const char* config_file, ServerConfig* config) {
    FILE* fp = fopen(config_file, "r");
    if (fp == NULL) {
        return -1;
    }
    
    char line[256];
    char key[64];
    char value[192];
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        
        // Parse key=value
        if (sscanf(line, "%63[^=]=%191[^\n]", key, value) == 2) {
            // Trim whitespace
            char* key_start = key;
            while (*key_start == ' ' || *key_start == '\t') key_start++;
            char* key_end = key_start + strlen(key_start) - 1;
            while (key_end > key_start && (*key_end == ' ' || *key_end == '\t')) {
                *key_end = '\0';
                key_end--;
            }
            
            char* value_start = value;
            while (*value_start == ' ' || *value_start == '\t') value_start++;
            char* value_end = value_start + strlen(value_start) - 1;
            while (value_end > value_start && (*value_end == ' ' || *value_end == '\t' || *value_end == '\n')) {
                *value_end = '\0';
                value_end--;
            }
            
            // Set configuration values
            if (strcmp(key_start, "PORT") == 0) {
                config->port = atoi(value_start);
            } else if (strcmp(key_start, "THREAD_POOL_SIZE") == 0) {
                config->thread_pool_size = atoi(value_start);
            } else if (strcmp(key_start, "MAX_CONNECTIONS") == 0) {
                config->max_connections = atoi(value_start);
            } else if (strcmp(key_start, "LOG_LEVEL") == 0) {
                config->log_level = parse_log_level(value_start);
            } else if (strcmp(key_start, "LOG_FILE") == 0) {
                strncpy(config->log_file, value_start, sizeof(config->log_file) - 1);
                config->log_file[sizeof(config->log_file) - 1] = '\0';
            }
        }
    }
    
    fclose(fp);
    return 0;
}
