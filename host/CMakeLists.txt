cmake_minimum_required(VERSION 3.15)
project(moonmic-host VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect platform (can be overridden by parent CMakeLists)
if(NOT DEFINED HOST_TARGET_WINDOWS AND NOT DEFINED HOST_TARGET_LINUX)
    if(WIN32)
        set(HOST_TARGET_WINDOWS ON)
    elseif(UNIX AND NOT APPLE)
        set(HOST_TARGET_LINUX ON)
    endif()
endif()

# Options
option(USE_IMGUI "Build with Dear ImGui GUI" ON)

# Source files
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/sunshine_integration.cpp
    src/audio_receiver.cpp
    src/codec/opus_decoder.cpp
    src/network/udp_receiver.cpp
)

# Platform-specific sources
if(HOST_TARGET_WINDOWS OR (WIN32 AND NOT HOST_TARGET_LINUX))
    message(STATUS "moonmic-host: Building for Windows")
    list(APPEND SOURCES
        src/platform/windows/virtual_device_win.cpp
        src/platform/windows/driver_installer.cpp
        src/platform/windows/gui_win.cpp
    )
elseif(HOST_TARGET_LINUX OR (UNIX AND NOT APPLE AND NOT HOST_TARGET_WINDOWS))
    message(STATUS "moonmic-host: Building for Linux")
    list(APPEND SOURCES
        src/platform/linux/virtual_device_linux.cpp
        src/platform/linux/gui_linux.cpp
    )
endif()

# Dear ImGui sources (if enabled)
if(USE_IMGUI)
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
    list(APPEND SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    add_definitions(-DUSE_IMGUI)
endif()

# Create executable
add_executable(moonmic-host ${SOURCES})

# Include directories
target_include_directories(moonmic-host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/codec
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
)

if(USE_IMGUI)
    target_include_directories(moonmic-host PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
endif()

# Find dependencies
find_package(Threads REQUIRED)
target_link_libraries(moonmic-host PRIVATE Threads::Threads)

# Opus
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(OPUS opus)
endif()

if(OPUS_FOUND)
    target_link_libraries(moonmic-host PRIVATE ${OPUS_LIBRARIES})
    target_include_directories(moonmic-host PRIVATE ${OPUS_INCLUDE_DIRS})
else()
    find_library(OPUS_LIBRARY NAMES opus)
    find_path(OPUS_INCLUDE_DIR opus/opus.h)
    if(OPUS_LIBRARY AND OPUS_INCLUDE_DIR)
        target_link_libraries(moonmic-host PRIVATE ${OPUS_LIBRARY})
        target_include_directories(moonmic-host PRIVATE ${OPUS_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "Opus library not found")
    endif()
endif()

# nlohmann/json (header-only)
find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(moonmic-host PRIVATE nlohmann_json::nlohmann_json)
else()
    # Fallback: use bundled json.hpp
    target_include_directories(moonmic-host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
endif()

# Platform-specific dependencies
if(HOST_TARGET_WINDOWS OR (WIN32 AND NOT HOST_TARGET_LINUX))
    target_link_libraries(moonmic-host PRIVATE
        ws2_32
        winmm
        ole32
    )
    
    # GLFW for ImGui
    if(USE_IMGUI)
        find_package(glfw3 REQUIRED)
        target_link_libraries(moonmic-host PRIVATE glfw opengl32)
    endif()
    
elseif(HOST_TARGET_LINUX OR (UNIX AND NOT APPLE AND NOT HOST_TARGET_WINDOWS))
    # PulseAudio
    pkg_check_modules(PULSEAUDIO REQUIRED libpulse-simple)
    target_link_libraries(moonmic-host PRIVATE ${PULSEAUDIO_LIBRARIES})
    target_include_directories(moonmic-host PRIVATE ${PULSEAUDIO_INCLUDE_DIRS})
    
    # GLFW for ImGui
    if(USE_IMGUI)
        find_package(glfw3 REQUIRED)
        find_package(OpenGL REQUIRED)
        target_link_libraries(moonmic-host PRIVATE glfw OpenGL::GL ${CMAKE_DL_LIBS})
    endif()
endif()

# Installation
install(TARGETS moonmic-host DESTINATION bin)
install(FILES config/moonmic-host.json.example DESTINATION etc RENAME moonmic-host.json)

# Copy config to build directory for testing
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/moonmic-host.json.example
    ${CMAKE_BINARY_DIR}/moonmic-host.json
    COPYONLY
)
