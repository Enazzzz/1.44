# Cross-compile 1.44 as a Windows x64 CLAP (renamed DLL) from Linux with mingw-w64.
# Prefer the posix-threaded GCC so std::mutex / libstdc++ match the static runtime.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(ONE44_MINGW_PREFIX "x86_64-w64-mingw32" CACHE STRING "MinGW target triplet")

set(CMAKE_C_COMPILER "${ONE44_MINGW_PREFIX}-gcc-posix")
set(CMAKE_CXX_COMPILER "${ONE44_MINGW_PREFIX}-g++-posix")
set(CMAKE_RC_COMPILER "${ONE44_MINGW_PREFIX}-windres")

set(CMAKE_FIND_ROOT_PATH "/usr/${ONE44_MINGW_PREFIX}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
