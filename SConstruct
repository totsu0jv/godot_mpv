#!/usr/bin/env python
import os
import sys

# Initialize environment from godot-cpp
env = SConscript("godot-cpp/SConstruct")

sources = Glob("src/*.cpp")

# --- CROSS-PLATFORM HEADERS ---
# Add MPV and GLAD headers
env.Append(CPPPATH=[
    "dependencies/mpv-dev/include",
    "dependencies/glad/include",
    "dependencies/mpv-build/mpv/include" # For your local Linux build
])

if env["platform"] == "windows":
    # Link against the import library downloaded in the Action
    env.Append(LIBPATH=["dependencies/mpv-dev"])
    env.Append(LIBS=["mpv.dll"]) # This looks for libmpv.dll.a

elif env["platform"] == "linux":
    # Use pkg-config (best for Linux)
    try:
        env.ParseConfig("pkg-config mpv --cflags --libs")
    except:
        # Fallback for your manual mpv-build setup
        env.Append(LIBPATH=['dependencies/mpv-build/mpv/build'])
        env.Append(LIBS=['mpv'])
    
    env.Append(LIBS=["GL", "EGL", "GLESv2"])

elif env["platform"] == "android":
    arch_path = "arm64-v8a" if env["arch"] == "arm64" else env["arch"]
    env.Append(LIBPATH=["bin/android/{}".format(arch_path)])
    env.Append(LIBS=["mpv"])

# Build the final library
library = env.SharedLibrary(
    "demo/bin/godot_mpv.{}.{}{}".format(env["platform"], env["arch"], env["suffix"]),
    source=sources,
)

Default(library)