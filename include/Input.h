#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Input {
public:
    // For now, callbacks: monitoring/debugging events (debug build only)
    // Helper functions: actual input state checking

    // Initialize with window (call once at startup)
    static void Init(GLFWwindow* pWin);

    // Input utility functions (self-explanatory)
    static bool IsKeyPressed(int key);       // Continuous pressing
    static bool WasKeyJustPressed(int key);  // Single-event presses
    static bool WasKeyJustReleased(int key);
    static bool IsMousePressed(int button);
    static void GetMousePos(double& xPos, double& yPos);
    static double GetMouseX();
    static double GetMouseY();

    // Getter functions
    static int GetWindowWidth() { return m_windowWidth; }
    static int GetWindowHeight() { return m_windowHeight; }
    static double GetScrollX() { return m_scrollX; }
    static double GetScrollY() { return m_scrollY; }

    // Sets up all GLFW event callbacks
    static void SetupEventCallbacks();

    // Called when GLFW encounters an error
    static void ErrorCallback(int error, char const* description);

    // Debug purposes
    static void PrintSpecs(); // Prints OpenGL system info

private:
    static GLFWwindow* m_window;

    // Event-based input tracking arrays
    static bool m_keysPressed[GLFW_KEY_LAST];
    static bool m_keysJustPressed[GLFW_KEY_LAST];
    static bool m_keysJustReleased[GLFW_KEY_LAST];

    // Tracking window width and height + scroll offsets
    static int m_windowWidth;
    static int m_windowHeight;
    static double m_scrollX;
    static double m_scrollY;

    // GLFW callback functions (input/window events)
    // Private functions must still have the exact function signatures that GLFW expects
    static void _windowSizeCallback(GLFWwindow* pWin, int width, int height);   // Window resize
    static void _keyCallback(GLFWwindow* pWin, int key, int scancode, int action, int mod); // Key press/release
    static void _mouseButtonCallback(GLFWwindow* pWin, int button, int action, int mod);    // Mouse button press/release
    static void _mousePosCallback(GLFWwindow* pWin, double xPos, double yPos);              // Mouse cursor movement
    static void _mouseScrollCallback(GLFWwindow* pWin, double xOffset, double yOffset);     // Mouse wheel scroll
};

// Constexpr variables (for key codes if needed) - I might park this in another file (like Keys.hpp or smt)
constexpr int KEY_W = GLFW_KEY_W;
constexpr int KEY_A = GLFW_KEY_A;
constexpr int KEY_S = GLFW_KEY_S;
constexpr int KEY_D = GLFW_KEY_D;
constexpr int MOUSE_LEFT = GLFW_MOUSE_BUTTON_LEFT;
constexpr int MOUSE_RIGHT = GLFW_MOUSE_BUTTON_RIGHT;
constexpr int PRESS = GLFW_PRESS;
constexpr int REPEAT = GLFW_REPEAT;
constexpr int RELEASE = GLFW_RELEASE;
