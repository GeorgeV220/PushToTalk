cmake_minimum_required(VERSION 3.29)
project(PushToTalk)

set(CMAKE_CXX_STANDARD 20)

# --- Packages ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(APPINDICATOR REQUIRED appindicator3-0.1)
pkg_check_modules(PULSEAUDIO REQUIRED libpulse)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)
pkg_check_modules(SPA REQUIRED IMPORTED_TARGET libspa-0.2)
find_package(OpenAL REQUIRED)
find_package(ZLIB REQUIRED)

find_library(MPG123_LIBRARIES mpg123 REQUIRED)
find_path(MPG123_INCLUDE_DIR mpg123.h REQUIRED)

if (NOT MPG123_LIBRARIES OR NOT MPG123_INCLUDE_DIR)
    message(FATAL_ERROR "mpg123 not found!")
endif ()

# --- Shared Sources ---
set(SHARED_SOURCES
        src/common/utilities/Utility.cpp
        src/common/utilities/Utility.h
        src/common/utilities/numbers/Conversion.cpp
        src/common/utilities/numbers/Conversion.h
        src/common/device/DeviceCapabilities.cpp
        src/common/device/DeviceCapabilities.h
)

# --- Client Executable ---
add_executable(ptt-client
        ${SHARED_SOURCES}
        src/client/client_main.cpp
        src/client/gui/SettingsGUI.cpp
        src/client/gui/SettingsGUI.h
        src/client/InputClient.cpp
        src/client/InputClient.h
        src/client/utilities/AudioUtilities.cpp
        src/client/utilities/AudioUtilities.h
        src/client/utilities/Settings.cpp
        src/client/utilities/Settings.h
        src/client/PushToTalkApp.cpp
        src/client/PushToTalkApp.h
        src/client/utilities/VirtualMicrophone.cpp
        src/client/utilities/VirtualMicrophone.h
)

target_include_directories(ptt-client
        PRIVATE
        src
        ${GTK3_INCLUDE_DIRS}
        ${APPINDICATOR_INCLUDE_DIRS}
        ${PULSEAUDIO_INCLUDE_DIRS}
        ${SPA_INCLUDE_DIRS}
        ${PIPEWIRE_INCLUDE_DIRS}
        ${OpenAL_INCLUDE_DIR}
        ${MPG123_INCLUDE_DIR}
)

target_link_libraries(ptt-client
        PRIVATE
        ${GTK3_LIBRARIES}
        ${APPINDICATOR_LIBRARIES}
        ${PULSEAUDIO_LIBRARIES}
        ${PIPEWIRE_LIBRARIES}
        ${SPA_LIBRARIES}
        ${OPENAL_LIBRARY}
        ${MPG123_LIBRARIES}
)

# --- Server Executable ---
add_executable(ptt-server
        ${SHARED_SOURCES}
        src/server/server_main.cpp
        src/server/device/VirtualInputProxy.cpp
        src/server/device/VirtualInputProxy.h
        src/server/InputProxyServer.cpp
        src/server/InputProxyServer.h
        src/server/cli/CommandLine.cpp
        src/server/cli/CommandLine.h
)

target_include_directories(ptt-server
        PRIVATE
        src
)

target_link_libraries(ptt-server
        PRIVATE
        ZLIB::ZLIB
)
