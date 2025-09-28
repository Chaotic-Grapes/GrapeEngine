#ifndef DEBUGUI_H
#define DEBUGUI_H

#include <vector>
#include <string>
#include "ecs/Entity.h"

// Forward declarations
struct GLFWwindow;
class World;

// Just for UI display (real data is in ECS components)
struct GameObject {
    EntityId Id;
    std::string Name;
    bool IsActive = true;

    GameObject(EntityId id, const std::string& name) : Id(id), Name(name) {}
};

class DebugUI {
public:
    static void Initialize(GLFWwindow* pWin);  // Call after OpenGL context exists
    static void NewFrame();  // Start new ImGUI frame before creating UI elements
    static void Render();    // Render all debug windows and send to GPU 
    static void Shutdown();  // Cleanup
    static void SetEnabled(bool enabled) { m_enabled = enabled; } // Enable or disable entire debug UI
    static bool IsEnabled() { return m_enabled; } // Check if UI is currently enabled

    // Set the world reference so we can create/manage entities
    static void SetWorld(World* world);

    // Game object management (find, add, remove)
    static std::vector<GameObject>& GetGameObjects() { return m_gameObjects; }
    static GameObject* FindGameObject(EntityId id);
    static void AddGameObject(const std::string& name);
    static void RemoveGameObject(EntityId id);

private:
    // World reference for entity creation/management
    static World* m_world;

    // Game object storage
    static std::vector<GameObject> m_gameObjects;
    static bool m_enabled;  // Control whether UI is visible or hidden

    static void _showEngineDebugWindow(bool& showDemo); // Main debug console window with engine status
    static void _showPerformanceWindow();  // Performance monitoring window
    static void _showInputDebugWindow();   // Input debugging
    static void _showGameObjectEditor();   // Game object editor window

    // Helper to create entities with basic components
    static Entity _createGameEntity(const std::string& name);
};

#endif
