cmake_minimum_required(VERSION 3.15)

project(godot_mpv)

# Paths
set(GODOT_CPP_PATH "engine/godot-cpp")
set(PROJECT_NAME godot_mpv)
set(ANGLE_ROOT "${CMAKE_SOURCE_DIR}/dependencies/angle")

# Detect platform
if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(PLATFORM android)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(PLATFORM windows)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM linux)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(PLATFORM macos)
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Git versioning
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")
include(GitVersion)
get_version_from_git()
configure_file(
    ${CMAKE_SOURCE_DIR}/native/src/godot_mpv/include/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/native/src/godot_mpv/include/version.h
)

# Godot-cpp binding
add_subdirectory(${GODOT_CPP_PATH} godot-cpp-build)

# Source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    native/src/godot_mpv/*.cpp
    native/src/godot_mpv/*.h
)

# --- ANGLE includes & libraries ---
# Only after target is created

# Specific libs for EGL and GLES on Windows
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Glad for EGL/GLES
    add_library(glad_gles2 STATIC
        "${CMAKE_SOURCE_DIR}/dependencies/glad/src/gles2.c"
    )
    target_include_directories(glad_gles2 PUBLIC
        "${CMAKE_SOURCE_DIR}/dependencies/glad/include"
    )

    add_library(glad_egl STATIC
        "${CMAKE_SOURCE_DIR}/dependencies/glad/src/egl.c"
    )
    target_include_directories(glad_egl PUBLIC
        "${CMAKE_SOURCE_DIR}/dependencies/glad/include"
    )

    # Main project target
    add_library(${PROJECT_NAME} SHARED ${SOURCES})

    # Compile features
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

    # Include directories
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/dependencies/glad
        ${CMAKE_SOURCE_DIR}/dependencies/mpv-dev/include
        ${GODOT_CPP_PATH}/include
        ${ANGLE_ROOT}/include
        src
    )

    # Link libraries
    target_link_directories(${PROJECT_NAME} PRIVATE
        ${ANGLE_ROOT}/lib
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        godot-cpp
        "${CMAKE_SOURCE_DIR}/dependencies/mpv-dev/libmpv.dll.a"
        glad_gles2
        glad_egl
        EGL
        GLESv2
    )

else()
    # Non-Windows platforms
    add_library(glad_gles2 SHARED
        "${CMAKE_SOURCE_DIR}/dependencies/glad/src/gles2.c"
    )
    target_include_directories(glad_gles2 PUBLIC
        "${CMAKE_SOURCE_DIR}/dependencies/glad/include"
    )

    add_library(glad_egl SHARED
        "${CMAKE_SOURCE_DIR}/dependencies/glad/src/egl.c"
    )
    target_include_directories(glad_egl PUBLIC
        "${CMAKE_SOURCE_DIR}/dependencies/glad/include"
    )

    add_library(${PROJECT_NAME} SHARED ${SOURCES})

    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/dependencies/glad
        ${GODOT_CPP_PATH}/include
        src
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        godot-cpp
        glad_gles2
        glad_egl
        mpv
    )
endif()

# Determine architecture
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH x86_64)
else()
    set(ARCH x86_32)
endif()

# Determine build type
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(BUILD_SUFFIX template_debug)
else()
    set(BUILD_SUFFIX template_release)
endif()

# Compose final output name
set(OUTPUT_FILENAME "${PROJECT_NAME}.${PLATFORM}.${BUILD_SUFFIX}.${ARCH}")

# Platform-specific settings
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${OUTPUT_FILENAME}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/godot_project/bin/
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/godot_project/bin/
)
