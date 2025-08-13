# This file configures CMake for cross-compiling from Linux to Windows
# using the MinGW-w64 toolchain.

# Set the target system for cross-compilation
set(CMAKE_SYSTEM_NAME Windows)

# Specify the cross-compilers for C and C++
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

# Specify the resource compiler for Windows resources
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Set the path to the target environment, used for finding headers and libraries
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Adjust CMake's find behavior for cross-compilation
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
