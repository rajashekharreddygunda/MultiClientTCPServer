#ifndef PROTOCOL_H
#define PROTOCOL_H

// Process a client command and generate response
// Returns 0 on success, -1 on error, 1 if client should disconnect
int process_command(const char* command, char* response, int response_size, int* active_clients);

#endif // PROTOCOL_H
