#ifndef DEBUGUI_H
#define DEBUGUI_H
#include <vector>
#include <string>
#include "System.h"
#include "Audio.h"
#include <unordered_map>
#include "ecs/Entity.h"

// Forward declarations (avoid unnecessary includes)
struct GLFWwindow;
class World;

// Configuration for DebugUI
struct DebugUIConfig {
    float FontScale = 1.35f;

    // Window positions and sizes
    struct WindowLayout {
        float EngineX = 10.0f, EngineY = 10.0f;
        float EngineW = 300.0f, EngineH = 150.0f;

        float InputX = 330.0f, InputY = 10.0f;
        float InputW = 320.0f, InputH = 400.0f;

        float EditorX = 670.0f, EditorY = 10.0f;
        float EditorW = 375.0f, EditorH = 400.0f;

        float PerfX = 1065.0f, PerfY = 10.0f;
        float PerfW = 300.0f, PerfH = 400.0f;
    } Layout;

    // Buffer sizes
    static constexpr size_t MAX_OBJECT_NAME_LENGTH = 128;
};

// Just for UI display (real data is in ECS components)
struct GameObject {
    EntityId Id;
    std::string Name;
    bool IsActive = true;
    float X = 0.0f, Y = 0.0f;

    GameObject(EntityId id, const std::string& name) : Id(id), Name(name) {}
};

class DebugUI {
public:
    // 2 args: ref to World obj and ref to config struct
    explicit DebugUI(World* world, const DebugUIConfig& config = {});
    ~DebugUI();

    // Core functionality
    void Initialize(GLFWwindow* pWin);  // Call after OpenGL context exists
    void NewFrame();  // Start new ImGUI frame before creating UI elements
    void Render();    // Render all debug windows and send to GPU 
    void Shutdown();  // Cleanup

    // State management
    void SetEnabled(bool enabled) { m_enabled = enabled; } // Enable or disable entire debug UI
    bool IsEnabled() const { return m_enabled; } // Check if UI is currently enabled

    // Set the world reference so we can create/manage entities
    void SetWorld(World* world) { m_world = world; }
    bool HasValidWorld() const;

    // Audio management
    static void AttachAudio(Systems::Audio * audio);
    static void DetachAudio();

    // Game object management (find, add, remove)
    const std::vector<GameObject>& GetGameObjects() const { return m_gameObjects; }
    GameObject* FindGameObject(EntityId id);
    void AddGameObject(const std::string& name);
    void RemoveGameObject(EntityId id);
    void ClearAllGameObjects();
    void SyncWithWorld();

private:
    // Configuration and state
    DebugUIConfig m_config;
    World* m_world;          // World reference for entity creation/management
    std::vector<GameObject> m_gameObjects;
    bool m_enabled = false;  // Control whether UI is visible or hidden
    bool m_initialized = false;

    // UI state
    bool m_showDemo = false;
    std::string m_newObjectName = "NewObject";

    // Event counters for input debugging
    int m_spacePressed = 0;
    int m_spaceReleased = 0;

    // Cached button labels to avoid string creation every frame
    // Avoid doing stuff like Delete##something
    // Mutable because the methods that access these caches are marked const
    mutable std::unordered_map<EntityId, std::string> m_cachedToggleLabels;
    mutable std::unordered_map<EntityId, std::string> m_cachedDeleteLabels;

    // UI rendering methods
    void _showEngineDebugWindow();  // Main debug console window with engine status
    void _showPerformanceWindow();  // Performance monitoring window
    void _showInputDebugWindow();   // Input debugging
    void _showGameObjectEditor();   // Game object editor window
    static void _showAudioWindow(Systems::Audio& audio);  // Audio editor window

    // Helper to create entities with basic components
    Entity _createGameEntity(const std::string& name);
    void _invalidateButtonCache();
    const std::string& _getToggleLabel(const GameObject& obj) const;
    const std::string& _getDeleteLabel(const GameObject& obj) const;
};

#endif