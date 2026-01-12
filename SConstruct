#!/usr/bin/env python
import os
import sys

# Initialize the environment from godot-cpp
env = SConscript("godot-cpp/SConstruct")

# Identify sources
sources = Glob("src/*.cpp")

# --- GLOBAL MPV CONFIG ---
# This matches the 'thirdparty' folder created in the GitHub Action
env.Append(CPPPATH=["thirdparty/mpv/include"])

# --- PLATFORM SPECIFIC LINKING ---

if env["platform"] == "windows":
    # Shinchiro SDK puts the .a/lib files in the same dir as the dll in our Action
    env.Append(LIBPATH=["bin/windows"])
    env.Append(LIBS=["mpv.dll"]) # SCons will look for libmpv.dll.a or mpv.lib

elif env["platform"] == "linux":
    # On Linux, we use the system-installed libmpv-dev via pkg-config
    # This is the most reliable way to get headers and libs on Ubuntu
    env.ParseConfig("pkg-config mpv --cflags --libs")
    env.Append(LIBS=["GL"])

elif env["platform"] == "macos":
    env.Append(LIBPATH=["bin/macos"])
    env.Append(LIBS=["mpv"])
    env.Append(LINKFLAGS=["-Wl,-rpath,@loader_path"])

elif env["platform"] == "android":
    # The architecture (arm64-v8a) is passed in from our matrix
    android_arch_dir = "arm64-v8a" if env["arch"] == "arm64" else env["arch"]
    env.Append(LIBPATH=["bin/android/{}".format(android_arch_dir)])
    env.Append(LIBS=["mpv"])

# --- BUILD TARGET ---

# We use the platform and arch in the filename to prevent overwriting
target_path = "demo/bin/godot_mpv.{}.{}{}".format(
    env["platform"], 
    env["arch"], 
    env["suffix"]
)

library = env.SharedLibrary(
    target_path,
    source=sources,
)

Default(library)