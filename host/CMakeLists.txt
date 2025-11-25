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
    src/single_instance.cpp
)

# Platform-specific sources
if(HOST_TARGET_WINDOWS OR (WIN32 AND NOT HOST_TARGET_LINUX))
    message(STATUS "moonmic-host: Building for Windows")
    list(APPEND SOURCES
        src/platform/windows/virtual_device_win.cpp
        src/platform/windows/driver_installer.cpp
        src/platform/windows/resources.rc
    )
elseif(HOST_TARGET_LINUX OR (UNIX AND NOT APPLE AND NOT HOST_TARGET_WINDOWS))
    message(STATUS "moonmic-host: Building for Linux")
    list(APPEND SOURCES
        src/platform/linux/virtual_device_linux.cpp
    )
endif()

# Dear ImGui sources (if enabled)
if(USE_IMGUI)
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
    
    if(EXISTS ${IMGUI_DIR}/imgui.cpp)
        list(APPEND SOURCES
            ${IMGUI_DIR}/imgui.cpp
            ${IMGUI_DIR}/imgui_draw.cpp
            ${IMGUI_DIR}/imgui_tables.cpp
            ${IMGUI_DIR}/imgui_widgets.cpp
            ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        )
        add_definitions(-DUSE_IMGUI)
        
        # Include directories for ImGui will be added after target creation
    else()
        message(WARNING "ImGui submodule not found at ${IMGUI_DIR}. Disabling GUI.")
        set(USE_IMGUI OFF)
    endif()
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

if(USE_IMGUI AND EXISTS ${IMGUI_DIR}/imgui.cpp)
    target_include_directories(moonmic-host PRIVATE
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )
endif()


# Find dependencies
find_package(Threads REQUIRED)
target_link_libraries(moonmic-host PRIVATE Threads::Threads)

# nlohmann/json (Header only)
set(JSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json)
if(EXISTS ${JSON_DIR}/include/nlohmann/json.hpp)
    target_include_directories(moonmic-host PRIVATE ${JSON_DIR}/include)
else()
    message(FATAL_ERROR "nlohmann/json not found in ${JSON_DIR}")
endif()

# GLFW (Windows only)
if(HOST_TARGET_WINDOWS OR (WIN32 AND NOT HOST_TARGET_LINUX))
    set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw)
    if(EXISTS ${GLFW_DIR}/CMakeLists.txt)
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        add_subdirectory(${GLFW_DIR} glfw)
        target_link_libraries(moonmic-host PRIVATE glfw)
    else()
        message(FATAL_ERROR "GLFW not found in ${GLFW_DIR}")
    endif()
endif()

# Opus
if(HOST_TARGET_WINDOWS OR (WIN32 AND NOT HOST_TARGET_LINUX))
    # Build Opus from source for Windows
    set(OPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus)
    if(EXISTS ${OPUS_DIR}/CMakeLists.txt)
        # Disable Opus examples and tests
        set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
        set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
        set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
        
        add_subdirectory(${OPUS_DIR} opus)
        target_link_libraries(moonmic-host PRIVATE opus)
        # Add both directories so both include paths work
        target_include_directories(moonmic-host PRIVATE 
            ${OPUS_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/opus
        )
    else()
        message(FATAL_ERROR "Opus submodule not found at ${OPUS_DIR}")
    endif()
else()
    # Use system Opus for Linux
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
endif()

# Platform-specific dependencies
if(HOST_TARGET_WINDOWS OR (WIN32 AND NOT HOST_TARGET_LINUX))
    target_link_libraries(moonmic-host PRIVATE
        ws2_32
        winmm
        ole32
        propsys
    )
    
    # Use console subsystem so we can see stdout/stderr
    # Also link statically to avoid DLL dependencies
    target_link_options(moonmic-host PRIVATE 
        -mconsole
        -static-libgcc
        -static-libstdc++
        -static
    )
    
    # GLFW for ImGui (already linked via submodule)
    if(USE_IMGUI)
        target_link_libraries(moonmic-host PRIVATE opengl32)
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
