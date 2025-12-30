#!/bin/bash

echo "Copmiling tcp_server"
g++ -o tcp_server tcp_server.cpp tcp_server_func.cpp -lpthread
echo "Copmiling tcp_client"
g++ -o tcp_client tcp_client.cpp tcp_client_func.cpp 
echo "Compilation finished. Starting server..."
./tcp_server