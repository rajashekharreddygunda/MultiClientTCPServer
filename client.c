/*
 * Simple TCP client for testing the server
 * Usage: ./client <command>
 * Example: ./client "PING"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 4096

int main(int argc, char* argv[]) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    
    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        printf("Examples:\n");
        printf("  %s PING\n", argv[0]);
        printf("  %s \"ECHO Hello World\"\n", argv[0]);
        printf("  %s TIME\n", argv[0]);
        printf("  %s STATS\n", argv[0]);
        return 1;
    }
    
    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return 1;
    }
    
    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }
    
    // Send command
    snprintf(buffer, sizeof(buffer), "%s\n", argv[1]);
    if (send(sock, buffer, strlen(buffer), 0) < 0) {
        perror("send");
        close(sock);
        return 1;
    }
    
    // Receive response
    memset(buffer, 0, sizeof(buffer));
    ssize_t bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received < 0) {
        perror("recv");
        close(sock);
        return 1;
    }
    
    // Print response
    printf("%s", buffer);
    
    // Close socket
    close(sock);
    
    return 0;
}
