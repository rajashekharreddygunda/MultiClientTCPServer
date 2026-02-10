#ifndef CONFIG_H
#define CONFIG_H

#include "logger.h"

typedef struct {
    int port;
    int thread_pool_size;
    int max_connections;
    LogLevel log_level;
    char log_file[256];
} ServerConfig;

// Load configuration from file
int config_load(const char* config_file, ServerConfig* config);

// Set default configuration values
void config_set_defaults(ServerConfig* config);

#endif // CONFIG_H
