#include "protocol.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#define MAX_COMMAND_LEN 1024

static void trim_newline(char* str) {
    int len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
    if (len > 1 && str[len - 2] == '\r') {
        str[len - 2] = '\0';
    }
}

int process_command(const char* command, char* response, int response_size, int* active_clients) {
    char cmd_copy[MAX_COMMAND_LEN];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    // Remove trailing newline
    trim_newline(cmd_copy);
    
    log_message(LOG_DEBUG, "Processing command: %s", cmd_copy);
    
    // PING command
    if (strcmp(cmd_copy, "PING") == 0) {
        snprintf(response, response_size, "PONG\n");
        return 0;
    }
    
    // TIME command
    if (strcmp(cmd_copy, "TIME") == 0) {
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
        snprintf(response, response_size, "%s\n", time_str);
        return 0;
    }
    
    // ECHO command
    if (strncmp(cmd_copy, "ECHO ", 5) == 0) {
        const char* message = cmd_copy + 5;
        snprintf(response, response_size, "%s\n", message);
        return 0;
    }
    
    // STATS command
    if (strcmp(cmd_copy, "STATS") == 0) {
        snprintf(response, response_size, "Active clients: %d\n", *active_clients);
        return 0;
    }
    
    // QUIT command
    if (strcmp(cmd_copy, "QUIT") == 0) {
        snprintf(response, response_size, "Goodbye\n");
        return 1; // Signal to close connection
    }
    
    // Unknown command
    snprintf(response, response_size, "ERROR: Unknown command\n");
    return 0;
}
