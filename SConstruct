#!/usr/bin/env python
import os
import sys

# Initialize environment from godot-cpp
env = SConscript("godot-cpp/SConstruct")

sources = Glob("src/*.cpp")

# All platforms use the same header folder we set up in the workflow
env.Append(CPPPATH=["thirdparty/mpv/include"])

if env["platform"] == "windows":
    env.Append(LIBPATH=["bin/windows"])
    env.Append(LIBS=["mpv.dll"])

elif env["platform"] == "linux":
    # Linux is easiest via pkg-config
    env.ParseConfig("pkg-config mpv --cflags --libs")
    env.Append(LIBS=["GL"])

elif env["platform"] == "macos":
    env.Append(LIBPATH=["bin/macos"])
    env.Append(LIBS=["mpv"])
    env.Append(LINKFLAGS=["-Wl,-rpath,@loader_path"])

elif env["platform"] == "android":
    # Handle the arm64 architecture path
    arch_path = "arm64-v8a" if env["arch"] == "arm64" else env["arch"]
    env.Append(LIBPATH=["bin/android/{}".format(arch_path)])
    env.Append(LIBS=["mpv"])

# Build the shared library inside the demo bin folder
library = env.SharedLibrary(
    "demo/bin/godot_mpv.{}.{}{}".format(env["platform"], env["arch"], env["suffix"]),
    source=sources,
)

Default(library)