# Screen Light

A simple Windows utility that turns your monitor into a soft, adjustable light source, perfect for illuminating your face during video conferences, while also preventing the system from sleeping.

## Features

- **Video Conference Lighting**: Provides a bright, full-screen white display to act as a key light, improving video quality in low-light environments.
- **Adjustable Brightness**: Use the `Up` and `Down` arrow keys to change the brightness of the screen light.
- **Keeps System Awake**: Continuously moves the mouse cursor in the background to prevent the system from locking or the monitor from turning off.
- **Minimalist Design**: Creates a fullscreen, borderless window with no distracting elements and a hidden cursor.
- **Standalone Executable**: Builds a single, portable `.exe` file with no external dependencies, thanks to static linking. It can be run from any Windows machine.
- **Console Integration**: Built as a console application for robust terminal interaction. When run by double-clicking, the console window is automatically hidden for a clean user experience.
- **Verbose Logging**: Use the `--verbose` flag in a terminal (like PowerShell or `cmd`) to see diagnostic messages printed directly to that terminal.
- **Easy Controls**: Adjust brightness coarsely or finely and quit the application with simple keyboard commands.

## Prerequisites

This project is set up for cross-compilation from a Linux environment to Windows. You will need the following tools installed:

- CMake (version 3.20 or newer)
- Ninja Build System
- MinGW-w64 Toolchain (specifically the `x86_64-w64-mingw32` target)

On Debian/Ubuntu-based systems, you can install these with:
```bash
sudo apt-get update
sudo apt-get install cmake ninja-build g++-mingw-w64-x86-64
```

## Building

The project uses CMake Presets for a simplified and reproducible build process.

1.  **Configure the project** (for a release build):
    ```bash
    cmake --preset mingw-release
    ```

2.  **Build the executable**:
    ```bash
    cmake --build --preset release
    ```

The final executable, `ScreenLight.exe`, will be located in the `build/mingw-release/` directory.

## Usage

Copy `ScreenLight.exe` to any Windows machine and run it.

- **Standard Mode**:
  ```
  ScreenLight.exe
  ```
  This will launch the fullscreen white window and start moving the mouse. Press `ESC` to quit.

- **Verbose Mode**:
  ```
  ScreenLight.exe --verbose
  ```
  This will launch the application and also open a separate console window to display log messages. Press `ESC` to quit, or `Ctrl+C` in the console window.

> [!TIP]
> Use the `Up` and `Down` arrow keys to change the brightness of the screen light.
> To achieve a finer brightness control, hold `Shift` when pressing `Up` and `Down`,