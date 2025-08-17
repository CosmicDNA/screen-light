// C++ Standard Library
#include <new>       // Required for placement new, used by std::string/vector in C++20+
#include <chrono>    // For std/::chrono for type-safe time durations
#include <cstdio>    // For freopen_s to redirect streams
#include <cstdlib>
#include <iostream>
#include <string>    // For std::string to parse command-line arguments
#include <vector>    // For std::vector to hold arguments

// Windows API - Include last, with macros to reduce header size and avoid conflicts.
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // For core Windows API functions
#include <shellapi.h> // For CommandLineToArgvW
#include "resource.h" // For our application icon ID

// Custom message used to signal a graceful shutdown from the console handler.
#define WM_APP_SHUTDOWN (WM_APP + 1)
// Timer ID for the mouse movement timer.
#define IDT_MOUSEMOVE_TIMER 1

bool g_isVerbose = false; // Global flag to control logging output.
HWND g_hMainWnd = NULL;   // Global handle to the main window for cross-thread communication.

// A simple logger that only prints messages if in verbose mode.
void logMessage(const std::string& message) {
    if (g_isVerbose) {
        std::cout << message << std::endl;
    }
}

// Use a dedicated namespace for configuration constants to keep them organized and reusable.
namespace config {
    constexpr int kInitialX = 100;
    constexpr int kInitialY = 100;
    constexpr int kVelocity = 2;
    // Using UINT for timer duration to match SetTimer's expectations. ~60 FPS
    constexpr UINT kFrameDelayMs = 16;
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
        logMessage("MouseMover initialized. Screen: " + std::to_string(screenWidth) + "x" + std::to_string(screenHeight));
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

    void toggle() {
        m_enabled = !m_enabled;
        logMessage(m_enabled ? "Mouse movement enabled." : "Mouse movement disabled.");
    }

    [[nodiscard]] bool is_enabled() const {
        return m_enabled;
    }

private:
    int screenWidth, screenHeight, x, y, dx, dy;
    bool m_enabled = true; // Movement is enabled by default.
};

// Forward declaration of the window procedure.
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL WINAPI ConsoleHandler(DWORD);

// Updates the window's background color to a new shade of gray.
void UpdateBackgroundColor(HWND hwnd, bool goLighter, int step) {
    // Retrieve the current background brush from the window class to determine the current color.
    HBRUSH hOldBrush = (HBRUSH)GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND);
    BYTE currentGrayLevel = 255; // Default to white if retrieval fails.

    if (hOldBrush) {
        LOGBRUSH lb;
        if (GetObject(hOldBrush, sizeof(LOGBRUSH), &lb)) {
            // Assuming the color is a shade of gray, so R, G, and B components are equal.
            currentGrayLevel = GetRValue(lb.lbColor);
        }
    }

    int newGrayLevel = currentGrayLevel;

    if (goLighter) {
        newGrayLevel += step;
        if (newGrayLevel > 255) newGrayLevel = 255;
    } else { // goDarker
        newGrayLevel -= step;
        if (newGrayLevel < 0) newGrayLevel = 0;
    }

    if (newGrayLevel != currentGrayLevel) {
        BYTE newGrayByte = static_cast<BYTE>(newGrayLevel);
        // Create a new brush with the updated color
        HBRUSH hNewBrush = CreateSolidBrush(RGB(newGrayByte, newGrayByte, newGrayByte));
        if (hNewBrush) {
            // Set the new brush as the background for the window class.
            HBRUSH hReplacedBrush = (HBRUSH)SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)hNewBrush);
            if (hReplacedBrush) DeleteObject(hReplacedBrush);
            // Force the window to repaint with the new background
            InvalidateRect(hwnd, NULL, TRUE);
            logMessage("Screen brightness set to " + std::to_string(newGrayByte) + "/255");
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
            int wlen = lstrlenW(argvW[i]);
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, argvW[i], wlen, NULL, 0, NULL, NULL);
            std::string arg(size_needed, 0);
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

// Handles console control events (like Ctrl+C) for graceful shutdown in verbose mode.
BOOL WINAPI ConsoleHandler(DWORD ctrlType) {
    switch (ctrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
        logMessage("Console event received. Shutting down gracefully...");
        if (g_hMainWnd) {
            // Post a message to the window's message queue to trigger a clean shutdown.
            // This is safer than calling DestroyWindow directly from this handler thread.
            PostMessage(g_hMainWnd, WM_APP_SHUTDOWN, 0, 0);
        }
        // Give the main thread a moment to process the message before the process terminates.
        Sleep(1000);
        return TRUE; // We handled the event.
    }
    return FALSE; // We did not handle the event; pass it to the next handler.
}

// The application entry point. By using main(), we create a console application.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const auto args = ParseCommandLine();
    setup_verbosity_from_args(args);

    if (g_isVerbose) {
        // Try to attach to the parent process's console. If that fails (e.g., launched
        // from the GUI), allocate a new console for logging. This makes --verbose robust.
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            AllocConsole();
        }
        // Redirect standard streams to the newly available console using the secure freopen_s.
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        std::ios::sync_with_stdio();
        // Register our handler for console events like Ctrl+C.
        SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    }

    const wchar_t CLASS_NAME[] = L"ScreenLighteerWindowClass";

    const BYTE initialGrayLevel = 255; // Start with a full white screen.
    HBRUSH hInitialBrush = CreateSolidBrush(RGB(initialGrayLevel, initialGrayLevel, initialGrayLevel));
    if (!hInitialBrush) {
        MessageBox(NULL, L"Could not create initial background brush.", L"Startup Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wc.hbrBackground = hInitialBrush;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Could not register window class.", L"Startup Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Screen Lighteer",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL,
        NULL,
        hInstance,
        NULL
    );

    g_hMainWnd = hwnd;

    if (!hwnd) {
        MessageBox(NULL, L"Could not create window.", L"Startup Error", MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    // Prevent the system from sleeping or turning off the display. This is the correct
    // way to keep the screen on, replacing the unreliable mouse-moving method.
    std::string message;
    if (SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)) {
        message = "Power management inhibited. System and display will stay on.";
    } else {
        message = "Warning: Could not inhibit power management.";
    }

    logMessage(message);

    // Standard message loop. GetMessage is more efficient than PeekMessage
    // as it yields the CPU until a message is available.
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Restore the system's normal power-saving behavior before exiting.
    SetThreadExecutionState(ES_CONTINUOUS);
    logMessage("Program terminated.");

    // If we created a console, free it before exiting.
    if (g_isVerbose) {
        FreeConsole();
    }

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // A static instance of MouseMover, created on the first call to WndProc.
    // It persists for the lifetime of the application.
    static MouseMover mover;

    switch (msg) {
    case WM_CREATE:
        // Set a timer to fire periodically, triggering mouse movement.
        // This is more efficient than a busy-wait loop.
        SetTimer(hwnd, IDT_MOUSEMOVE_TIMER, config::kFrameDelayMs, NULL);
        return EXIT_SUCCESS;

    case WM_APP_SHUTDOWN:
        // This custom message is sent from our console handler. Fall through to WM_CLOSE.
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return EXIT_SUCCESS;

    case WM_DESTROY:
        {
            // Ensure the cursor is visible again when the application closes.
            ShowCursor(TRUE);
            KillTimer(hwnd, IDT_MOUSEMOVE_TIMER);
            HBRUSH hBrush = (HBRUSH)GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND);
            if (hBrush) {
                DeleteObject(hBrush);
            }
        }
        PostQuitMessage(0);
        return EXIT_SUCCESS;

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
                case 'M': // Toggle mouse movement
                    mover.toggle(); // Toggle the movement state...
                    ShowCursor(mover.is_enabled()); // ...and sync cursor visibility with it.
                    break;
            }
        }
        return EXIT_SUCCESS;

    case WM_TIMER:
        if (wParam == IDT_MOUSEMOVE_TIMER && mover.is_enabled()) {
            mover.update();
        }
        return EXIT_SUCCESS;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}