cmake_minimum_required(VERSION 3.29)
project(pttcpp)

set(CMAKE_CXX_STANDARD 20)

add_executable(pttcpp src/main.cpp
        src/Utility.h
        src/Utility.cpp
        src/Settings.h
        src/Settings.cpp
        src/InputHandler.cpp
        src/InputHandler.h)


find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

add_definitions(${GTK3_CFLAGS_OTHER})

target_link_libraries(pttcpp ${GTK3_LIBRARIES})
