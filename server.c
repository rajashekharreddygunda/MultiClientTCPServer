#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "config.h"
#include "logger.h"
#include "thread_pool.h"
#include "protocol.h"

#define BUFFER_SIZE 4096

// Global variables for signal handling
static volatile sig_atomic_t server_running = 1;
static int server_socket = -1;

// Thread-safe active client counter
static int active_clients = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Structure to pass client info to worker threads
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} ClientInfo;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (signal == SIGINT) {
        log_message(LOG_INFO, "Received SIGINT, shutting down...");
        server_running = 0;
        
        // Close server socket to unblock accept()
        if (server_socket != -1) {
            close(server_socket);
        }
    }
}

// Increment active client count
void increment_active_clients(void) {
    pthread_mutex_lock(&clients_mutex);
    active_clients++;
    pthread_mutex_unlock(&clients_mutex);
}

// Decrement active client count
void decrement_active_clients(void) {
    pthread_mutex_lock(&clients_mutex);
    active_clients--;
    pthread_mutex_unlock(&clients_mutex);
}

// Get active client count
int get_active_clients(void) {
    int count;
    pthread_mutex_lock(&clients_mutex);
    count = active_clients;
    pthread_mutex_unlock(&clients_mutex);
    return count;
}

// Handle client connection (executed by worker thread)
void handle_client(void* arg) {
    ClientInfo* client_info = (ClientInfo*)arg;
    int client_socket = client_info->client_socket;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_info->client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_info->client_addr.sin_port);
    
    increment_active_clients();
    log_message(LOG_INFO, "Client connected: %s:%d (Active: %d)", 
                client_ip, client_port, get_active_clients());
    
    char buffer[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    
    while (1) {
        // Receive data from client
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                log_message(LOG_INFO, "Client disconnected: %s:%d", client_ip, client_port);
            } else {
                log_message(LOG_ERROR, "recv() failed for %s:%d: %s", 
                           client_ip, client_port, strerror(errno));
            }
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        // Process command
        int active_count = get_active_clients();
        int result = process_command(buffer, response, sizeof(response), &active_count);
        
        // Send response
        ssize_t bytes_sent = send(client_socket, response, strlen(response), 0);
        if (bytes_sent < 0) {
            log_message(LOG_ERROR, "send() failed for %s:%d: %s", 
                       client_ip, client_port, strerror(errno));
            break;
        }
        
        // Check if client should disconnect
        if (result == 1) {
            log_message(LOG_INFO, "Client requested disconnect: %s:%d", client_ip, client_port);
            break;
        }
    }
    
    // Close client socket
    close(client_socket);
    decrement_active_clients();
    log_message(LOG_DEBUG, "Connection closed: %s:%d (Active: %d)", 
                client_ip, client_port, get_active_clients());
    
    free(client_info);
}

int main(int argc, char* argv[]) {
    ServerConfig config;
    ThreadPool* pool = NULL;
    
    // Set default configuration
    config_set_defaults(&config);
    
    // Load configuration from file if provided
    const char* config_file = (argc > 1) ? argv[1] : "config.txt";
    if (config_load(config_file, &config) == 0) {
        printf("Loaded configuration from %s\n", config_file);
    } else {
        printf("Using default configuration\n");
    }
    
    // Initialize logger
    const char* log_file = (strlen(config.log_file) > 0) ? config.log_file : NULL;
    logger_init(log_file, config.log_level);
    
    log_message(LOG_INFO, "Starting TCP server...");
    log_message(LOG_INFO, "Port: %d", config.port);
    log_message(LOG_INFO, "Thread pool size: %d", config.thread_pool_size);
    log_message(LOG_INFO, "Max connections: %d", config.max_connections);
    
    // Setup signal handler
    signal(SIGINT, signal_handler);
    
    // Create thread pool
    pool = thread_pool_create(config.thread_pool_size);
    if (pool == NULL) {
        log_message(LOG_ERROR, "Failed to create thread pool");
        logger_close();
        return EXIT_FAILURE;
    }
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        log_message(LOG_ERROR, "socket() failed: %s", strerror(errno));
        thread_pool_destroy(pool);
        logger_close();
        return EXIT_FAILURE;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_message(LOG_ERROR, "setsockopt() failed: %s", strerror(errno));
        close(server_socket);
        thread_pool_destroy(pool);
        logger_close();
        return EXIT_FAILURE;
    }
    
    // Bind socket
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(config.port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        log_message(LOG_ERROR, "bind() failed: %s", strerror(errno));
        close(server_socket);
        thread_pool_destroy(pool);
        logger_close();
        return EXIT_FAILURE;
    }
    
    // Listen for connections
    if (listen(server_socket, config.max_connections) < 0) {
        log_message(LOG_ERROR, "listen() failed: %s", strerror(errno));
        close(server_socket);
        thread_pool_destroy(pool);
        logger_close();
        return EXIT_FAILURE;
    }
    
    log_message(LOG_INFO, "Server listening on port %d", config.port);
    
    // Main accept loop
    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_socket < 0) {
            if (server_running) {
                log_message(LOG_ERROR, "accept() failed: %s", strerror(errno));
            }
            break;
        }
        
        // Allocate client info structure
        ClientInfo* client_info = (ClientInfo*)malloc(sizeof(ClientInfo));
        if (client_info == NULL) {
            log_message(LOG_ERROR, "malloc() failed for client info");
            close(client_socket);
            continue;
        }
        
        client_info->client_socket = client_socket;
        client_info->client_addr = client_addr;
        
        // Add task to thread pool
        if (thread_pool_add_task(pool, handle_client, client_info) < 0) {
            log_message(LOG_ERROR, "Failed to add task to thread pool");
            close(client_socket);
            free(client_info);
        }
    }
    
    // Cleanup
    log_message(LOG_INFO, "Shutting down server...");
    
    if (server_socket != -1) {
        close(server_socket);
    }
    
    thread_pool_destroy(pool);
    
    log_message(LOG_INFO, "Server stopped. Total active clients at shutdown: %d", 
                get_active_clients());
    
    logger_close();
    
    return EXIT_SUCCESS;
}
