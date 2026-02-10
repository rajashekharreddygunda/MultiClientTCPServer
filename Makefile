# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -O2
LDFLAGS = -pthread

# Target executables
SERVER_TARGET = server
CLIENT_TARGET = client

# Source files
SERVER_SOURCES = server.c thread_pool.c logger.c config.c protocol.c
SERVER_OBJECTS = $(SERVER_SOURCES:.c=.o)

CLIENT_SOURCES = client.c
CLIENT_OBJECTS = $(CLIENT_SOURCES:.c=.o)

# Header files
HEADERS = thread_pool.h logger.h config.h protocol.h

# Default target
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# Link object files to create executables
$(SERVER_TARGET): $(SERVER_OBJECTS)
	$(CC) $(SERVER_OBJECTS) -o $(SERVER_TARGET) $(LDFLAGS)
	@echo "Build complete: $(SERVER_TARGET)"

$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	$(CC) $(CLIENT_OBJECTS) -o $(CLIENT_TARGET) $(LDFLAGS)
	@echo "Build complete: $(CLIENT_TARGET)"

# Compile source files to object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(SERVER_OBJECTS) $(CLIENT_OBJECTS) $(SERVER_TARGET) $(CLIENT_TARGET)
	rm -f server.log
	@echo "Clean complete"

# Run the server
run: $(SERVER_TARGET)
	./$(SERVER_TARGET)

# Run with valgrind for memory leak detection
valgrind: $(SERVER_TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(SERVER_TARGET)

# Help target
help:
	@echo "Available targets:"
	@echo "  all       - Build the server and client (default)"
	@echo "  clean     - Remove build artifacts"
	@echo "  run       - Build and run the server"
	@echo "  valgrind  - Run server with valgrind memory checker"
	@echo "  help      - Show this help message"

.PHONY: all clean run valgrind help
