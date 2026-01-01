cmake_minimum_required(VERSION 3.10)
project(NetworkMonitor)

set(CMAKE_CXX_STANDARD 17)

find_package(SQLite3 REQUIRED)

include_directories(include)

add_executable(tcp_server
    app/tcp_server.cpp
    lib/tcp_server_func.cpp
    lib/SQLite_manager.cpp
    app/udp_server.cpp
)

target_link_libraries(tcp_server pthread SQLite::SQLite3)

add_executable(tcp_client
    app/tcp_client.cpp
    lib/tcp_client_func.cpp
)

target_link_libraries(tcp_client pthread)