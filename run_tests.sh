#!/bin/bash
# Simple server test script

# Start server in background
./server &
SERVER_PID=$!

# Wait for server to start
sleep 2

echo "Testing server..."
echo ""

echo "1. Testing PING:"
./client PING

echo ""
echo "2. Testing TIME:"
./client TIME

echo ""
echo "3. Testing ECHO:"
./client "ECHO Hello from test script"

echo ""
echo "4. Testing STATS:"
./client STATS

echo ""
echo "5. Testing unknown command:"
./client INVALID

# Stop server
echo ""
echo "Stopping server..."
kill -INT $SERVER_PID
wait $SERVER_PID 2>/dev/null

echo "Test complete!"
