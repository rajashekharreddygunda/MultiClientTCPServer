#!/usr/bin/env python3
"""
Automated test script for the TCP server
Tests all protocol commands and concurrent connections
"""

import socket
import threading
import time
import sys

# Server configuration
SERVER_HOST = 'localhost'
SERVER_PORT = 8080
TIMEOUT = 5.0

class TestResults:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.errors = []
    
    def add_pass(self, test_name):
        self.passed += 1
        print(f"✓ {test_name}")
    
    def add_fail(self, test_name, reason):
        self.failed += 1
        error = f"✗ {test_name}: {reason}"
        self.errors.append(error)
        print(error)
    
    def summary(self):
        total = self.passed + self.failed
        print("\n" + "="*60)
        print(f"Test Results: {self.passed}/{total} passed")
        if self.failed > 0:
            print(f"\nFailed tests:")
            for error in self.errors:
                print(f"  {error}")
        print("="*60)
        return self.failed == 0

def send_command(command, host=SERVER_HOST, port=SERVER_PORT):
    """Send a command to the server and return the response"""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(TIMEOUT)
            s.connect((host, port))
            s.sendall(command.encode() + b'\n')
            response = s.recv(4096).decode().strip()
            return response
    except Exception as e:
        raise Exception(f"Connection error: {e}")

def test_ping(results):
    """Test PING command"""
    try:
        response = send_command("PING")
        if response == "PONG":
            results.add_pass("PING command")
        else:
            results.add_fail("PING command", f"Expected 'PONG', got '{response}'")
    except Exception as e:
        results.add_fail("PING command", str(e))

def test_time(results):
    """Test TIME command"""
    try:
        response = send_command("TIME")
        # Just check that we got a response (format validation would be complex)
        if len(response) > 0 and '-' in response and ':' in response:
            results.add_pass("TIME command")
        else:
            results.add_fail("TIME command", f"Invalid time format: '{response}'")
    except Exception as e:
        results.add_fail("TIME command", str(e))

def test_echo(results):
    """Test ECHO command"""
    try:
        test_message = "Hello, World!"
        response = send_command(f"ECHO {test_message}")
        if response == test_message:
            results.add_pass("ECHO command")
        else:
            results.add_fail("ECHO command", f"Expected '{test_message}', got '{response}'")
    except Exception as e:
        results.add_fail("ECHO command", str(e))

def test_stats(results):
    """Test STATS command"""
    try:
        response = send_command("STATS")
        if "Active clients:" in response:
            results.add_pass("STATS command")
        else:
            results.add_fail("STATS command", f"Invalid stats format: '{response}'")
    except Exception as e:
        results.add_fail("STATS command", str(e))

def test_quit(results):
    """Test QUIT command"""
    try:
        response = send_command("QUIT")
        if "Goodbye" in response:
            results.add_pass("QUIT command")
        else:
            results.add_fail("QUIT command", f"Expected 'Goodbye', got '{response}'")
    except Exception as e:
        results.add_fail("QUIT command", str(e))

def test_unknown_command(results):
    """Test unknown command handling"""
    try:
        response = send_command("INVALID_COMMAND")
        if "ERROR" in response or "Unknown" in response:
            results.add_pass("Unknown command handling")
        else:
            results.add_fail("Unknown command handling", f"Expected error, got '{response}'")
    except Exception as e:
        results.add_fail("Unknown command handling", str(e))

def concurrent_client(client_id, num_commands, results_lock, success_count):
    """Function for concurrent client test"""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(TIMEOUT)
            s.connect((SERVER_HOST, SERVER_PORT))
            
            for i in range(num_commands):
                message = f"Client{client_id}_Msg{i}"
                s.sendall(f"ECHO {message}\n".encode())
                response = s.recv(4096).decode().strip()
                
                if response != message:
                    return
            
            with results_lock:
                success_count[0] += 1
    except Exception as e:
        pass

def test_concurrent_connections(results, num_clients=10, commands_per_client=5):
    """Test multiple concurrent connections"""
    try:
        threads = []
        results_lock = threading.Lock()
        success_count = [0]
        
        for i in range(num_clients):
            t = threading.Thread(target=concurrent_client, 
                               args=(i, commands_per_client, results_lock, success_count))
            threads.append(t)
            t.start()
        
        for t in threads:
            t.join(timeout=TIMEOUT * 2)
        
        if success_count[0] == num_clients:
            results.add_pass(f"Concurrent connections ({num_clients} clients)")
        else:
            results.add_fail(f"Concurrent connections", 
                           f"Only {success_count[0]}/{num_clients} clients succeeded")
    except Exception as e:
        results.add_fail("Concurrent connections", str(e))

def test_persistent_connection(results):
    """Test multiple commands on a single connection"""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(TIMEOUT)
            s.connect((SERVER_HOST, SERVER_PORT))
            
            # Send multiple commands on same connection
            commands = [
                ("PING", "PONG"),
                ("ECHO Test123", "Test123"),
                ("STATS", "Active clients:"),
            ]
            
            all_passed = True
            for cmd, expected in commands:
                s.sendall(cmd.encode() + b'\n')
                response = s.recv(4096).decode().strip()
                if expected not in response:
                    all_passed = False
                    break
            
            if all_passed:
                results.add_pass("Persistent connection (multiple commands)")
            else:
                results.add_fail("Persistent connection", "Command responses incorrect")
    except Exception as e:
        results.add_fail("Persistent connection", str(e))

def check_server_running():
    """Check if server is running"""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(2.0)
            s.connect((SERVER_HOST, SERVER_PORT))
        return True
    except:
        return False

def main():
    print("="*60)
    print("TCP Server Test Suite")
    print("="*60)
    
    # Check if server is running
    if not check_server_running():
        print(f"\n✗ Error: Server is not running on {SERVER_HOST}:{SERVER_PORT}")
        print("Please start the server before running tests.")
        sys.exit(1)
    
    print(f"\n✓ Server is running on {SERVER_HOST}:{SERVER_PORT}\n")
    
    results = TestResults()
    
    # Run basic command tests
    print("Testing Basic Commands:")
    print("-" * 60)
    test_ping(results)
    test_time(results)
    test_echo(results)
    test_stats(results)
    test_quit(results)
    test_unknown_command(results)
    
    # Run connection tests
    print("\nTesting Connection Handling:")
    print("-" * 60)
    test_persistent_connection(results)
    test_concurrent_connections(results, num_clients=10)
    test_concurrent_connections(results, num_clients=20)
    
    # Print summary
    success = results.summary()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
