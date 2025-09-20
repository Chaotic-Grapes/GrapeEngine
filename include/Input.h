#pragma once
#include <unordered_map>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace Engine { class Application; } // Forward declaration for friend class
class Input {
public:
    // For now, callbacks: monitoring/debugging events (debug build only)
    // Helper functions: actual input state checking

    // Initialize with window (call once at startup)
    static void Initialize(GLFWwindow* pWin);

    // Input utility functions (self-explanatory)
    static bool IsKeyPressed(int key);
    static bool IsKeyDown(int key);
    static bool IsKeyUp(int key);
    static bool IsMousePressed(int button);
	//static bool IsMouseDown(int button);
	//static bool IsMouseUp(int button);
    static void GetMousePosition(double& xPos, double& yPos);
    static double GetMouseX();
    static double GetMouseY();

    // Sets up all GLFW event callbacks
    static void SetupEventCallbacks();

    // Called when GLFW encounters an error
    static void ErrorCallback(int error, char const* description);

    // Debug purposes
    static void PrintSpecs(); // Prints OpenGL system info

private:
    friend class Engine::Application;
    static GLFWwindow* m_window;

    static std::unordered_map<int, bool> m_keyDown;
    static std::unordered_map<int, bool> m_keyPressed;
    static std::unordered_map<int, bool> m_keyUp;

    // GLFW callback functions (input/window events)
    // Private functions must still have the exact function signatures that GLFW expects
    static void _framebufferSizeCallback(GLFWwindow* pWin, int width, int height);   // Window resize
    static void _keyCallback(GLFWwindow* pWin, int key, int scancode, int action, int mod); // Key press/release
    static void _mouseButtonCallback(GLFWwindow* pWin, int button, int action, int mod);    // Mouse button press/release
    static void _mousePosCallback(GLFWwindow* pWin, double xPos, double yPos);              // Mouse cursor movement
    static void _mouseScrollCallback(GLFWwindow* pWin, double xOffset, double yOffset);     // Mouse wheel scroll

    static void _processInput();
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
