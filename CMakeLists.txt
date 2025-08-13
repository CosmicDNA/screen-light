# Require a modern version of CMake
cmake_minimum_required(VERSION 3.20)

# Define the project name and language
project(ScreenLight LANGUAGES CXX)

# Set the C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create the executable from the source file
add_executable(${PROJECT_NAME} WIN32 src/screen_light.cpp)

# Link against necessary Windows libraries only when building for Windows
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Statically link runtime libraries to create a portable executable.
    # This avoids runtime errors like "libgcc_s_seh-1.dll was not found".
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    target_link_libraries(${PROJECT_NAME} PRIVATE user32 gdi32)
endif()