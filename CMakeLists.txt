cmake_minimum_required(VERSION 3.15)
project(godot_mpv)

# Replace with your Godot 4.x install path
set(GODOT_CPP_PATH "engine/godot-cpp")
set(PROJECT_NAME godot_mpv)

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

# Specific libs for EGL and GLES on windows
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")

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

    add_library(${PROJECT_NAME} SHARED ${SOURCES})
    target_link_libraries(${PROJECT_NAME} PRIVATE
        godot-cpp
        "${CMAKE_SOURCE_DIR}/dependencies/mpv-dev/libmpv.dll.a"
        glad_gles2
        glad_egl
        EGL
        GLESv2
        )

    # Compiler settings
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_INSTALL_PREFIX}/include
        ${CMAKE_SOURCE_DIR}/dependencies/glad
        ${CMAKE_SOURCE_DIR}/dependencies/mpv-dev/include
        ${GODOT_CPP_PATH}/include
        src
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")

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
    target_link_libraries(${PROJECT_NAME} PRIVATE
        godot-cpp
        glad_gles2
        glad_egl
        mpv
        )

    # Compiler settings
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/dependencies/glad
        ${GODOT_CPP_PATH}/include
        src
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    add_compile_definitions(GL_SILENCE_DEPRECATION)

    
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MPV REQUIRED mpv)
    
    add_library(${PROJECT_NAME} SHARED ${SOURCES})
    
    target_link_libraries(${PROJECT_NAME} PRIVATE
        godot-cpp
        ${MPV_LIBRARIES}
        "-framework OpenGL"
        "-framework CoreVideo"
        "-framework IOKit"
        "-framework Cocoa"
    )
    
    # Compiler settings
    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
    
    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${MPV_INCLUDE_DIRS}
        ${GODOT_CPP_PATH}/include
        src
    )
    
    # Add rpath for finding mpv library
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "@loader_path"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    
    # FIX: Ensure ANDROID_ABI is defined
    set(ANDROID_ABI ${CMAKE_ANDROID_ARCH_ABI})
    message(STATUS "Android ABI: ${ANDROID_ABI}")

    add_library(glad STATIC
        "${CMAKE_SOURCE_DIR}/dependencies/glad/src/egl.c"
        "${CMAKE_SOURCE_DIR}/dependencies/glad/src/gles2.c"
    )

    set_target_properties(glad PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    target_include_directories(glad PUBLIC
        "${CMAKE_SOURCE_DIR}/dependencies/glad/include"
    )
    
    # Set the correct library path based on Android ABI
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(MPV_LIB_PATH "${CMAKE_SOURCE_DIR}/dependencies/arm64-v8a")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(MPV_LIB_PATH "${CMAKE_SOURCE_DIR}/dependencies/armeabi-v7a")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(MPV_LIB_PATH "${CMAKE_SOURCE_DIR}/dependencies/x86_64")
    else()
        message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
    endif()
    
    link_directories(${MPV_LIB_PATH})

    add_library(${PROJECT_NAME} SHARED ${SOURCES})
    target_link_libraries(${PROJECT_NAME} PRIVATE
        godot-cpp
        glad
        mpv
        EGL
        GLESv3
    )

    target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

    target_include_directories(${PROJECT_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/dependencies/glad
        ${CMAKE_SOURCE_DIR}/dependencies/mpv-dev/include
        ${GODOT_CPP_PATH}/include
        src
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

# Compose the final output name
set(OUTPUT_FILENAME "${PROJECT_NAME}.${PLATFORM}.${BUILD_SUFFIX}.${ARCH}")

# Platform-specific settings
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ${OUTPUT_FILENAME}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/godot_project/bin/
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/godot_project/bin/
)