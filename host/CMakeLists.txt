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

# Generate version header from template
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)

# Source files
set(SOURCES
    src/codec/ffmpeg_decoder.cpp
    src/network/udp_receiver.cpp
    src/network/connection_monitor.cpp
    src/sunshine_integration.cpp
    src/config.cpp
    src/main.cpp
    src/audio_receiver.cpp
    src/sunshine_webui.cpp
    src/sunshine_settings_gui.cpp
    src/display_settings_gui.cpp
    src/single_instance.cpp
    src/display_manager.cpp
    src/version_checker.cpp
    src/debug_gui.cpp
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

# Include third_party FIRST (builds OpenSSL if available)
add_subdirectory(third_party)

# CRITICAL: Ensure all external dependencies are built BEFORE linking moonmic-host
# This prevents race conditions in parallel builds
if(TARGET curl_build)
    add_dependencies(moonmic-host curl_build)
endif()
if(TARGET ffmpeg_build)
    add_dependencies(moonmic-host ffmpeg_build)
endif()
if(TARGET speexdsp)
    add_dependencies(moonmic-host speexdsp)
endif()

# Include directories
target_include_directories(moonmic-host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/codec
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated version.h
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


# CURL for HTTP client (required for pairing)
# Use built CURL from third_party if available, otherwise use system
if(CURL_FOUND AND CURL_LIB_PATH)
    # Using built CURL from submodule with absolute path
    message(STATUS "Using built CURL from submodule")
    message(STATUS "  CURL lib: ${CURL_LIB_PATH}")
    target_link_libraries(moonmic-host PRIVATE ${CURL_LIB_PATH})
    target_include_directories(moonmic-host PRIVATE ${CURL_INCLUDE_DIRS})
    # Define CURL_STATICLIB for static linking on Windows
    if(WIN32)
        target_compile_definitions(moonmic-host PRIVATE CURL_STATICLIB)
    endif()
else()
    # Fallback to system CURL
    find_package(CURL REQUIRED)
    target_link_libraries(moonmic-host PRIVATE CURL::libcurl)
    message(STATUS "Using system CURL")
endif()

# Windows-specific system libraries for CURL with SChannel
if(WIN32)
    # CURL requires Winsock, LDAP, and SChannel libraries for HTTPS
    target_link_libraries(moonmic-host PRIVATE
        ws2_32     # Winsock2
        wldap32    # Windows LDAP (needed by CURL)
        iphlpapi   # IP Helper API (if_nametoindex for CURL)
        crypt32    # Windows Crypto API (SChannel)
        bcrypt     # Windows crypto primitives (SChannel)
    )
    message(STATUS "Linked Windows system libraries for CURL with SChannel")
else()
    # Linux: CURL uses system OpenSSL, need to link it
    find_package(OpenSSL REQUIRED)
    target_link_libraries(moonmic-host PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    message(STATUS "Linked system OpenSSL for CURL HTTPS support")
endif()

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

# FFmpeg (Static build from submodule)
# FFmpeg is built automatically from the git submodule at third_party/ffmpeg
message(STATUS "Configuring FFmpeg static build...")

# Link against static FFmpeg libraries
target_link_libraries(moonmic-host PRIVATE ${FFMPEG_LIBRARIES})
target_include_directories(moonmic-host PRIVATE ${FFMPEG_INCLUDE_DIRS})

# Windows: FFmpeg static libraries require additional system libraries
if(HOST_TARGET_WINDOWS OR (WIN32 AND NOT HOST_TARGET_LINUX))
    target_link_libraries(moonmic-host PRIVATE
        bcrypt      # For BCrypt* functions (random number generation)
        secur32     # For security functions
    )
endif()

message(STATUS "FFmpeg enabled (static build from submodule)")

# Speex Resampler (for high-quality 16kHz->48kHz)
if(HOST_TARGET_WINDOWS OR (WIN32 AND NOT HOST_TARGET_LINUX))
    # Build from source for Windows
    set(SPEEX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/speexdsp)
    if(EXISTS ${SPEEX_DIR}/libspeexdsp/resample.c)
        add_library(speexdsp STATIC
            ${SPEEX_DIR}/libspeexdsp/resample.c
        )
        
        target_include_directories(speexdsp PUBLIC
            ${SPEEX_DIR}/include
        )
        
        target_compile_definitions(speexdsp PRIVATE
            EXPORT=
            FLOATING_POINT
            USE_SSE
            USE_SSE2
        )
        
        target_link_libraries(moonmic-host PRIVATE speexdsp)
        target_include_directories(moonmic-host PRIVATE ${SPEEX_DIR}/include)
        message(STATUS "Speex resampler enabled (built from source)")
    else()
        message(WARNING "Speex submodule not found - audio quality may be degraded")
    endif()
else()
    # Use system speexdsp for Linux
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(SPEEXDSP speexdsp)
    endif()
    
    if(SPEEXDSP_FOUND)
        target_link_libraries(moonmic-host PRIVATE ${SPEEXDSP_LIBRARIES})
        target_include_directories(moonmic-host PRIVATE ${SPEEXDSP_INCLUDE_DIRS})
        message(STATUS "Speex resampler enabled (system library)")
    else()
        message(WARNING "Speex library not found - install libspeexdsp-dev")
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
    
    # GLFW for ImGui - use embedded submodule
    if(USE_IMGUI)
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
