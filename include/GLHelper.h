#ifndef GLHELPER_H
#define GLHELPER_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

struct GLHelper {
    // For now, callbacks: monitoring/debugging events (debug build only)
    // Helper functions: actual input state checking
    
    // Input utility functions (self-explanatory)
    static bool IsKeyPressed(GLFWwindow* pWin, int key);
    static bool IsMouseButtonPressed(GLFWwindow* pWin, int button);
    static void GetMousePosition(GLFWwindow* pWin, double& xPos, double& yPos);
    static double GetMouseX(GLFWwindow* pWin);
    static double GetMouseY(GLFWwindow* pWin);

    // Sets up all GLFW event callbacks
    static void SetupEventCallbacks(GLFWwindow* pWin);

    // GLFW callback functions (input/window events)
    static void ErrorCallback(int error, char const* description);         // Called when GLFW encounters an error
    static void FBSizeCallback(GLFWwindow* pWin, int width, int height);   // Window resize
    static void KeyCallback(GLFWwindow* pWin, int key, int scancode, int action, int mod); // Key press/release
    static void MouseButtonCallback(GLFWwindow* pWin, int button, int action, int mod);    // Mouse button press/release
    static void MousePosCallback(GLFWwindow* pWin, double xPos, double yPos);              // Mouse cursor movement
    static void MouseScrollCallback(GLFWwindow* pWin, double xOffset, double yOffset);     // Mouse wheel scroll

    // Debug purposes
    static void PrintSpecs(); // Prints OpenGL system info
};

#endif