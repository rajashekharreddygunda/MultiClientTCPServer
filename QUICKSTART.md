# Quick Start Guide

Get the server running in 60 seconds!

## 1. Build Everything

```bash
make
```

This compiles both the server and client test program.

## 2. Start the Server

```bash
./server
```

You should see:
```
Loaded configuration from config.txt
[2026-02-10 14:30:45] [INFO] Starting TCP server...
[2026-02-10 14:30:45] [INFO] Port: 8080
[2026-02-10 14:30:45] [INFO] Thread pool size: 4
[2026-02-10 14:30:45] [INFO] Server listening on port 8080
```

## 3. Test with the Client (New Terminal)

```bash
# In a new terminal window:
./client PING
# Output: PONG

./client "ECHO Hello World"
# Output: Hello World

./client TIME
# Output: 2026-02-10 14:30:45

./client STATS
# Output: Active clients: 1
```

## 4. Run Full Test Suite

```bash
./test_server.py
```

Expected output:
```
============================================================
TCP Server Test Suite
============================================================

✓ Server is running on localhost:8080

Testing Basic Commands:
------------------------------------------------------------
✓ PING command
✓ TIME command
✓ ECHO command
✓ STATS command
✓ QUIT command
✓ Unknown command handling

Testing Connection Handling:
------------------------------------------------------------
✓ Persistent connection (multiple commands)
✓ Concurrent connections (10 clients)
✓ Concurrent connections (20 clients)

============================================================
Test Results: 9/9 passed
============================================================
```

## 5. Try Interactive Mode with Telnet

```bash
telnet localhost 8080
```

Then type commands:
```
PING
TIME
ECHO Testing
STATS
QUIT
```

## 6. Stop the Server

Press `Ctrl+C` in the server terminal. You'll see:
```
[INFO] Received SIGINT, shutting down...
[INFO] Shutting down server...
[INFO] Thread pool destroyed
[INFO] Server stopped. Total active clients at shutdown: 0
```

## Common Issues

### "Address already in use"
Wait 30 seconds or change the port in `config.txt`

### "Connection refused"  
Make sure the server is running (`./server`)

### Tests fail
Ensure server is running before running `./test_server.py`

## Next Steps

- Read `README.md` for detailed documentation
- Modify `config.txt` to customize settings
- Check memory leaks: `make valgrind`
- Review the code to understand the architecture

## Example: Load Test

Create `load_test.py`:
```python
import socket
import threading

def client(n):
    s = socket.socket()
    s.connect(('localhost', 8080))
    for i in range(10):
        s.send(f'ECHO Client{n}_Msg{i}\n'.encode())
        print(s.recv(1024).decode().strip())
    s.close()

threads = [threading.Thread(target=client, args=(i,)) for i in range(20)]
for t in threads: t.start()
for t in threads: t.join()
print("Load test complete!")
```

Run it:
```bash
python3 load_test.py
```

Happy coding! 🚀
