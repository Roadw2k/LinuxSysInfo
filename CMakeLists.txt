cmake_minimum_required(VERSION 3.16)
project(UbuntuSysInfo VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_standard_project_setup()

qt_add_executable(LinuxSysInfo
    main.cpp
    sysinfowidget.h
    sysinfowidget.cpp
)

target_link_libraries(LinuxSysInfo PRIVATE Qt6::Widgets Qt6::Network)