#ifndef DEBUGUI_H
#define DEBUGUI_H

// Forward declare GLFWwindow to avoid including GLFW here
struct GLFWwindow;

class DebugUI {
public:
    static void Initialize(GLFWwindow* pWin);  // Call after OpenGL context exists
    static void NewFrame(); // Start new ImGUI frame before creating UI elements
    static void Render();   // Render all debug windows and send to GPU 
    static void Shutdown(); // Cleanup

    static void SetEnabled(bool enabled) { m_enabled = enabled; } // Enable or disable entire debug UI
    static bool IsEnabled() { return m_enabled; } // Check if UI is currently enabled

private:
    static bool m_enabled;
    static void _showEngineDebugWindow(bool& showDemo); // Main debug console window with engine status
    static void _showPerformanceWindow(); // Performance monitoring window
    static void _showInputDebugWindow();   // Input debugging
};

#endif
