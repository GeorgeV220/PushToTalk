cmake_minimum_required(VERSION 3.29)
project(pttcpp)

set(CMAKE_CXX_STANDARD 20)

add_executable(pttcpp
        src/main.cpp
        src/push_to_talk.cpp
        src/push_to_talk.h
        src/Utility.h
        src/Utility.cpp
        src/Settings.h
        src/Settings.cpp
        src/InputHandler.cpp
        src/InputHandler.h
        src/gui/SettingsGUI.cpp
        src/gui/SettingsGUI.h
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(${GTK3_INCLUDE_DIRS})
link_directories(${GTK3_LIBRARY_DIRS})

add_definitions(${GTK3_CFLAGS_OTHER})

find_package(OpenAL REQUIRED)
if (OpenAL_FOUND)
    include_directories(${OpenAL_INCLUDE_DIR})
else()
    message(FATAL_ERROR "OpenAL not found!")
endif()

find_library(MPG123_LIBRARIES mpg123 REQUIRED)
find_path(MPG123_INCLUDE_DIR mpg123.h REQUIRED)

if (MPG123_LIBRARIES AND MPG123_INCLUDE_DIR)
    include_directories(${MPG123_INCLUDE_DIR})
else()
    message(FATAL_ERROR "mpg123 not found!")
endif()

target_link_libraries(pttcpp
        ${GTK3_LIBRARIES}
        ${OPENAL_LIBRARY}
        ${MPG123_LIBRARIES}
)
