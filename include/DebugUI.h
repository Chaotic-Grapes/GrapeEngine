#ifndef DEBUGUI_H
#define DEBUGUI_H

#include <vector>
#include <string>
#include "System.h"
#include "Audio.h"

// Forward declare GLFWwindow to avoid including GLFW here
struct GLFWwindow;

// Simple GameObject structure for the editor
struct GameObject {
    int Id;
    std::string Name;
    float Position[3] = { 0.0f, 0.0f, 0.0f };  // x, y, z
    float Rotation[3] = { 0.0f, 0.0f, 0.0f };  // Same same
    float Scale[3] = { 1.0f, 1.0f, 1.0f };    
    bool IsActive = true;

    GameObject(int id, const std::string& name) : Id(id), Name(name) {}
};

class DebugUI {
public:
    static void Initialize(GLFWwindow* pWin);  // Call after OpenGL context exists
    static void NewFrame();  // Start new ImGUI frame before creating UI elements
    static void Render();    // Render all debug windows and send to GPU 
    static void AttachAudio(Systems::Audio * audio);
    static void DetachAudio();                     
    static void Shutdown();  // Cleanup
    static void SetEnabled(bool enabled) { m_enabled = enabled; } // Enable or disable entire debug UI
    static bool IsEnabled() { return m_enabled; } // Check if UI is currently enabled

    // Game object management (find, add, remove)
    static std::vector<GameObject>& GetGameObjects() { return m_gameObjects; }
    static GameObject* FindGameObject(int id);
    static void AddGameObject(const std::string& name);
    static void RemoveGameObject(int id);

private:
    // Game object storage
    static std::vector<GameObject> m_gameObjects;
    static int m_nextGameObjectId;

    static bool m_enabled;  // Control whether UI is visible or hidden
    static void _showEngineDebugWindow(bool& showDemo); // Main debug console window with engine status
    static void _showPerformanceWindow();  // Performance monitoring window
    static void _showInputDebugWindow();   // Input debugging
    static void _showGameObjectEditor();   // Game object editor window
    static void _showAudioWindow(Systems::Audio& audio);        // Audio editor window
};

#endif
