// C++ Standard Library
#include <new>       // Required for placement new, used by std::string/vector in C++20+
#include <atomic>    // For std::atomic
#include <chrono>    // For std::chrono for type-safe time durations
#include <cstdio>    // For freopen_s to redirect streams
#include <cstdlib>
#include <iostream>
#include <string>    // For std::string to parse command-line arguments
#include <thread>    // For std::this_thread::sleep_for
#include <vector>    // For std::vector to hold arguments

// Windows API - Include last, with macros to reduce header size and avoid conflicts.
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // For core Windows API functions
#include <shellapi.h> // For CommandLineToArgvW
#include "../res/resource.h" // For our application icon ID

std::atomic<bool> keepRunning = true;
bool g_isVerbose = false; // Global flag to control logging output.

// A simple logger that only prints messages if in verbose mode.
void logMessage(const std::string& message) {
    if (g_isVerbose) {
        // After redirecting stdout with freopen_s, C-style I/O is more reliable
        // than C++ streams, which may not sync correctly with the new console.
        fprintf(stdout, "%s\n", message.c_str());
        fflush(stdout); // Ensure the output is written immediately.
    }
}
// This is the console control handler function that will be called for events like Ctrl+C.
// It's the preferred, platform-native way to handle this on Windows.
BOOL WINAPI consoleHandler(DWORD ctrlType) {
    switch (ctrlType) {
    // Handle Ctrl+C, Ctrl+Break, and the console window being closed.
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        logMessage("\nShutdown signal received. Shutting down gracefully.");
        keepRunning = false;
        return TRUE; // Indicate that we have handled the event. The process will terminate after this handler returns.
    }
    return FALSE; // Pass on other events to the next handler.
}

// Use a dedicated namespace for configuration constants to keep them organized and reusable.
namespace config {
    constexpr int kInitialX = 100;
    constexpr int kInitialY = 100;
    constexpr int kVelocity = 2;
    constexpr auto kFrameDelay = std::chrono::milliseconds(5);
}

class MouseMover {
public:
    MouseMover()
        : screenWidth(GetSystemMetrics(SM_CXSCREEN)),
          screenHeight(GetSystemMetrics(SM_CYSCREEN)),
          x(config::kInitialX),
          y(config::kInitialY),
          dx(config::kVelocity),
          dy(config::kVelocity) {
        // Use std::to_string for compatibility, as some MinGW toolchains lack <format>.
        logMessage("Screen dimensions: " + std::to_string(screenWidth) + "x" + std::to_string(screenHeight));
    }

    void update() {
        // Move the cursor to the new (x, y) position.
        SetCursorPos(x, y);

        // Update the coordinates for the next position.
        x += dx;
        y += dy;

        // Bounce off the screen edges.
        if (x <= 0 || x >= screenWidth - 1) {
            dx = -dx; // Reverse horizontal direction
        }
        if (y <= 0 || y >= screenHeight - 1) {
            dy = -dy; // Reverse vertical direction
        }
    }

private:
    int screenWidth, screenHeight, x, y, dx, dy;
};

// Forward declaration of the window procedure.
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Parses the application's command line into a vector of strings.
std::vector<std::string> ParseCommandLine() {
    std::vector<std::string> args;
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argvW) {
        // Start from 1 to skip the program name, matching standard argv behavior.
        for (int i = 1; i < argc; ++i) {
            // Determine the length of the wide string to convert, excluding the null terminator.
            int wlen = lstrlenW(argvW[i]);
            // Determine the buffer size needed for the UTF-8 string.
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, argvW[i], wlen, NULL, 0, NULL, NULL);
            std::string arg(size_needed, 0);
            // Perform the conversion.
            WideCharToMultiByte(CP_UTF8, 0, argvW[i], wlen, &arg[0], size_needed, NULL, NULL);
            args.push_back(std::move(arg));
        }
        LocalFree(argvW);
    }
    return args;
}

void setup_console_from_args(const std::vector<std::string>& args) {
    for (const auto& arg : args) {
        if (arg == "--verbose") {
            g_isVerbose = true;
            AllocConsole();
            FILE* dummy;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            freopen_s(&dummy, "CONOUT$", "w", stderr);
            return; // Found the flag, console is set up, no need to check further.
        }
    }
}

// Use WinMain as the entry point to create a true GUI application that doesn't open a console by default.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Parse command-line arguments to check for flags like --verbose.
    const auto args = ParseCommandLine();
    setup_console_from_args(args);

    // Register our consoleHandler function.
    if (!SetConsoleCtrlHandler(consoleHandler, TRUE))
    {
        logMessage("Error: Could not set control handler.");
        MessageBoxW(NULL, L"Could not set console control handler.", L"Startup Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    const wchar_t CLASS_NAME[] = L"ScreenLightWindowClass";

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    // Load the icon from our resources
    wc.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        logMessage("Error: Could not register window class.");
        MessageBoxW(NULL, L"Could not register window class.", L"Startup Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    HWND hwnd = CreateWindowExW(
        0,                              // Optional window styles.
        CLASS_NAME,                     // Window class
        L"Screen Light",                // Window text
        WS_POPUP,                       // Window style: a borderless popup window
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), // Position and size
        NULL,                           // Parent window
        NULL,                           // Menu
        hInstance,                      // Instance handle
        NULL                            // Additional application data
    );

    if (!hwnd) {
        logMessage("Error: Could not create window.");
        MessageBoxW(NULL, L"Could not create window.", L"Startup Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    // ShowCursor(FALSE); // Hide the mouse cursor for a cleaner look.

    logMessage("Screen light started. Press ESC to stop the program.");

    MouseMover mover;
    MSG msg = {};

    // This is the main message loop.
    while (keepRunning.load()) {
        // Process all pending messages in the queue.
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                keepRunning = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!keepRunning.load()) {
            break;
        }

        // If there are no messages to process, do our work.
        mover.update();
        std::this_thread::sleep_for(config::kFrameDelay);
    }

    ShowCursor(TRUE); // Restore the mouse cursor before exiting.
    logMessage("Program terminated.");

    return (int)msg.wParam; // Return the exit code from WM_QUIT
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CLOSE:
        // User tried to close the window (e.g., Alt+F4).
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        // The window is being destroyed.
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        // User pressed a key.
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
        }
        return 0;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}