# Require a modern version of CMake
cmake_minimum_required(VERSION 3.20)

# Define the project name and language
project(ScreenLighteer VERSION 1.0.8 LANGUAGES CXX)

# Set the C++ standard to C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configure the resource file template to inject the project version.
# This creates a resource.rc file in the build directory with the correct version info.
configure_file(
    res/resource.rc.in
    ${CMAKE_CURRENT_BINARY_DIR}/resource.rc
)

# Create the executable from the source file
add_executable(${PROJECT_NAME} WIN32
    src/screen_light.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/resource.rc
)

# Add the 'res' directory to the include path. This allows the resource compiler
# (windres) to find "resource.h" when compiling the .rc file, and also allows
# the C++ compiler to find it from screen_light.cpp.
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/res)

# Define preprocessor macros for Unicode support across the application.
# This ensures Windows API calls correctly resolve to their wide-character (W) versions.
target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE)

# Link against necessary Windows libraries only when building for Windows
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # By commenting out the "-static" flag, we switch to dynamic linking.
    # This means the MinGW runtime DLLs (e.g., libstdc++-6.dll, libgcc_s_seh-1.dll)
    # must be distributed alongside the executable.
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    target_link_libraries(${PROJECT_NAME} PRIVATE user32 gdi32 shell32)
endif()