cmake_minimum_required(VERSION 3.10)
project(NetworkMonitor)

set(CMAKE_CXX_STANDARD 17)

find_package(SQLite3 REQUIRED)

add_executable(tcp_server
    tcp_server.cpp
    tcp_server_func.cpp
    SQLite_manager.cpp
    udp_server.cpp
)

target_link_libraries(tcp_server pthread SQLite::SQLite3)

add_executable(tcp_client
    tcp_client.cpp
    tcp_client_func.cpp
)

target_link_libraries(tcp_client pthread)