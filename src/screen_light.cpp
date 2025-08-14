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
#include "resource.h" // For our application icon ID

// Custom message used to signal a graceful shutdown from the console handler.
#define WM_APP_SHUTDOWN (WM_APP + 1)

bool g_isVerbose = false; // Global flag to control logging output.
BYTE g_grayLevel = 255;   // Current gray level (0=black, 255=white)
HWND g_hMainWnd = NULL;   // Global handle to the main window for cross-thread communication.

// A simple logger that only prints messages if in verbose mode.
void logMessage(const std::string& message) {
    if (g_isVerbose) {
        // By default, std::cout is synchronized with the C stdout stream.
        // After redirecting stdout with freopen_s, std::cout will write to the new console.
        // std::endl flushes the buffer, ensuring the message appears immediately.
        std::cout << message << std::endl;
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
        // Post a message to the main window's message queue to initiate shutdown.
        // This is the thread-safe way to communicate from the handler to the main loop.
        if (g_hMainWnd) {
            PostMessage(g_hMainWnd, WM_APP_SHUTDOWN, 0, 0);
        }
        // We've handled the signal. The main loop will now exit cleanly.
        return TRUE;
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

// Updates the window's background color to a new shade of gray.
void UpdateBackgroundColor(HWND hwnd, bool goLighter, int step) {
    int newGrayLevel = g_grayLevel;

    if (goLighter) {
        newGrayLevel += step;
        if (newGrayLevel > 255) newGrayLevel = 255;
    } else { // goDarker
        newGrayLevel -= step;
        if (newGrayLevel < 0) newGrayLevel = 0;
    }

    if (newGrayLevel != g_grayLevel) {
        g_grayLevel = static_cast<BYTE>(newGrayLevel);

        // Create a new brush with the updated color
        HBRUSH hNewBrush = CreateSolidBrush(RGB(g_grayLevel, g_grayLevel, g_grayLevel));
        if (hNewBrush) {
            // Set the new brush as the background for the window class.
            // The old brush is returned and must be deleted to prevent leaks.
            HBRUSH hOldBrush = (HBRUSH)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hNewBrush);
            if (hOldBrush) {
                DeleteObject(hOldBrush);
            }

            // Force the window to repaint with the new background
            InvalidateRect(hwnd, NULL, TRUE);
            logMessage("Screen brightness set to " + std::to_string(g_grayLevel) + "/255");
        }
    }
}

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

// Sets the verbose flag based on command-line arguments.
void setup_verbosity_from_args(const std::vector<std::string>& args) {
    for (const auto& arg : args) {
        if (arg == "--verbose") {
            g_isVerbose = true;
            return;
        }
    }
}

// The application entry point. By using main(), we create a console application.
int main() {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    const auto args = ParseCommandLine();
    setup_verbosity_from_args(args);

    // If not in verbose mode, hide the console window that this application was launched with.
    if (!g_isVerbose) {
        ShowWindow(GetConsoleWindow(), SW_HIDE);
    }

    // Register our consoleHandler function for graceful shutdown on Ctrl+C.
    if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
        // This is not a fatal error, but we should log it if possible.
        logMessage("Warning: Could not set control handler.");
    }

    const wchar_t CLASS_NAME[] = L"ScreenLightWindowClass";

    // Create the initial background brush (white).
    HBRUSH hInitialBrush = CreateSolidBrush(RGB(g_grayLevel, g_grayLevel, g_grayLevel));
    if (!hInitialBrush) {
        logMessage("Error: Could not create initial background brush.");
        MessageBox(NULL, L"Could not create initial background brush.", L"Startup Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    // Load the icon from our resources
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hbrBackground = hInitialBrush;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassEx(&wc)) {
        logMessage("Error: Could not register window class.");
        MessageBox(NULL, L"Could not register window class.", L"Startup Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    HWND hwnd = CreateWindowEx(
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

    g_hMainWnd = hwnd; // Store the main window handle for the console handler to use.

    if (!hwnd) {
        logMessage("Error: Could not create window.");
        MessageBox(NULL, L"Could not create window.", L"Startup Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    logMessage("Screen light started. Press ESC to stop the program.");

    MouseMover mover;
    MSG msg = {};

    // Main message loop. It continues until WM_QUIT is received.
    while (true) {
        // Process all pending messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                break; // Exit loop if WM_QUIT is received.
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // If there are no messages, do our work.
            mover.update();
            std::this_thread::sleep_for(config::kFrameDelay);
        }
    }

    logMessage("Program terminated.");

    return (int)msg.wParam; // Return the exit code from WM_QUIT
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_APP_SHUTDOWN:
        // This custom message is sent from our console handler. Fall through to WM_CLOSE.
    case WM_CLOSE:
        // User tried to close the window (e.g., Alt+F4).
        DestroyWindow(hwnd);
        break;

    case WM_DESTROY:
        {
            HBRUSH hBrush = (HBRUSH)GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND);
            if (hBrush) {
                DeleteObject(hBrush);
            }
        }
        PostQuitMessage(0);
        break;

    case WM_KEYDOWN:
        {
            const int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 1 : 10;
            switch (wParam) {
                case VK_ESCAPE:
                    DestroyWindow(hwnd);
                    break;
                case VK_UP:
                    UpdateBackgroundColor(hwnd, true, step);
                    break;
                case VK_DOWN:
                    UpdateBackgroundColor(hwnd, false, step);
                    break;
            }
        }
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return EXIT_SUCCESS; // Return SUCCESS for messages we've handled.
}