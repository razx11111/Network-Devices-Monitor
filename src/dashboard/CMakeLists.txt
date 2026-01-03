cmake_minimum_required(VERSION 3.10)
project(MonitorDashboard)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt 5 or 6
find_package(Qt5 COMPONENTS Widgets Network REQUIRED)

# Include the shared headers from your main project
include_directories(../include)

add_executable(dashboard
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(dashboard Qt5::Widgets Qt5::Network)