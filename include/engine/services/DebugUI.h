/**
 * @file    DebugUI.h
 * @author  Foo Rui Qin
 * @date    2025
 * @brief Debug user interface system for engine development and debugging
 * 
 * This file defines the DebugUI class which provides a comprehensive ImGui-based
 * debug interface for the game engine. Features include:
 * - Real-time engine status monitoring and performance metrics
 * - Interactive game object editor with entity management
 * - Input debugging and event tracking
 * - Audio system monitoring and control
 * - Configurable window layouts and UI scaling
 * - Integration with ECS (Entity Component System)
 * - Memory and profiling information display
 * - Toggle-able interface with F1 key
 */

#ifndef DEBUGUI_H
#define DEBUGUI_H
#include <vector>
#include <string>
#include <unordered_map>
#include "audio/FmodAudioDevice.h"
#include "ecs/Entity.h"

// Forward declarations (avoid unnecessary includes)
struct GLFWwindow;
class World;
namespace Audio { class IAudioDevice; }

/**
 * @brief Configuration structure for DebugUI appearance and layout
 * 
 * Contains settings for font scaling, window positions, sizes, and buffer limits
 * for the debug interface. Allows customization of the UI layout and appearance.
 */
struct DebugUIConfig {
    float FontScale = 1.35f;  ///< Global font scaling factor for the entire UI

    /**
     * @brief Window layout configuration for debug UI panels
     * 
     * Defines positions and sizes for all debug windows including engine status,
     * input debugging, game object editor, and performance monitor.
     */
    struct WindowLayout {
        float EngineX = 10.0f, EngineY = 10.0f;     ///< Engine debug window position
        float EngineW = 300.0f, EngineH = 175.0f;   ///< Engine debug window size

        float InputX = 330.0f, InputY = 10.0f;      ///< Input debug window position
        float InputW = 320.0f, InputH = 400.0f;     ///< Input debug window size

        float EditorX = 670.0f, EditorY = 10.0f;    ///< Game object editor window position
        float EditorW = 375.0f, EditorH = 400.0f;   ///< Game object editor window size

        float PerfX = 1065.0f, PerfY = 10.0f;       ///< Performance monitor window position
        float PerfW = 300.0f, PerfH = 400.0f;       ///< Performance monitor window size

        float AudioX = 10.0f, AudioY = 200.0f;
        float AudioW = 300.0f, AudioH = 207.0f;

        float ControlsX = 10.0f, ControlsY = 422.0f;
        float ControlsW = 300.0f, ControlsH = 200.0f;
    } Layout;

    static constexpr size_t MAX_OBJECT_NAME_LENGTH = 128;  ///< Maximum length for game object names
};

/**
 * @brief Debug user interface system for engine development
 * 
 * The DebugUI class provides a comprehensive ImGui-based debug interface that allows
 * developers to monitor engine status, edit game objects, debug input, and analyze
 * performance in real-time. It integrates with the ECS system and provides tools
 * for entity management and system monitoring.
 * 
 * Key features:
 * - Real-time engine status and performance monitoring
 * - Interactive game object creation, editing, and deletion
 * - Input event debugging and tracking
 * - Audio system monitoring and control
 * - Memory usage and profiling information
 * - Configurable UI layout and appearance
 * - Toggle-able interface (F1 key)
 * 
 * Usage example:
 * @code
 * DebugUI debugUI(&world);
 * debugUI.Initialize(window);
 * 
 * // In main loop:
 * debugUI.NewFrame();
 * debugUI.Render();
 * @endcode
 */
class DebugUI {
public:
    enum class GameState {
        Stopped,  // Editor mode
        Playing,  // Game running
        Paused    // Freeze
    };

    GameState GetGameState() const { return m_gameState; }
    bool IsPlaying() const { return m_gameState == GameState::Playing; }

    /**
     * @brief Constructor for DebugUI
     * @param world Pointer to the World object for entity management
     * @param config Configuration structure for UI layout and appearance
     * 
     * Initializes the debug UI with a reference to the world and optional
     * configuration settings. The world pointer is used for entity management.
     */
    explicit DebugUI(World* world, const DebugUIConfig& config = {});
    
    /**
     * @brief Destructor for DebugUI
     * 
     * Automatically calls Shutdown() if the UI was initialized to ensure
     * proper cleanup of ImGui resources.
     */
    ~DebugUI();

    /**
     * @brief Initialize the debug UI system
     * @param pWin GLFW window pointer for ImGui integration
     * 
     * Sets up ImGui context, configures backends for GLFW and OpenGL,
     * applies styling, and prepares the UI for rendering. Must be called
     * after OpenGL context creation.
     */
    void Initialize(GLFWwindow* pWin);
    
    /**
     * @brief Start a new ImGui frame
     * 
     * Prepares ImGui for a new frame by processing input events and
     * setting up rendering state. Should be called at the beginning
     * of each frame before creating UI elements.
     */
    void NewFrame();
    
    /**
     * @brief Render all debug windows and UI elements
     * 
     * Renders all debug windows including engine status, performance monitor,
     * input debugging, game object editor, and audio controls. Finalizes
     * the frame and submits draw commands to the GPU.
     */
    void Render();
    
    /**
     * @brief Shutdown and cleanup the debug UI
     * 
     * Properly shuts down ImGui backends, destroys the context, and
     * cleans up all resources. Called automatically by destructor.
     */
    void Shutdown();

    /**
     * @brief Enable or disable the entire debug UI
     * @param enabled True to enable UI, false to disable
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief Check if the debug UI is currently enabled
     * @return bool True if UI is enabled, false otherwise
     */
    bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Set the world reference for entity management
     * @param world Pointer to the World object
     * 
     * Updates the world reference used for creating and managing entities
     * through the debug interface.
     */
    void SetWorld(World* world) { m_world = world; }
    
    /**
     * @brief Check if a valid world reference exists
     * @return bool True if world pointer is valid, false otherwise
     */
    bool HasValidWorld() const;

    /**
     * @brief Attach an audio system for monitoring
     * @param device Pointer to the audio system
     * 
     * Static method to attach an audio system for monitoring and control
     * through the debug interface.
     */
    static void AttachAudio(Audio::FmodAudioDevice* device);

    /**
     * @brief Detach the audio system
     * 
     * Static method to detach the audio system from debug monitoring.
     */
    static void DetachAudio();

    /**
     * @brief Add a new game object to the world
     * @param name Name for the new game object
     * 
     * Creates a new entity with basic components and adds it to the world.
     * The object is positioned randomly within the window bounds.
     */
    void AddGameObject(const std::string& name);
    
    /**
     * @brief Remove a game object by entity ID
     * @param id Entity ID of the object to remove
     * 
     * Finds and destroys the entity with the specified ID, removing it
     * from the world and updating the UI cache.
     */
    void RemoveGameObject(EntityId id);
    
    /**
     * @brief Clone an existing game object
     * @param entity Entity to clone
     * 
     * Creates a copy of the specified entity with slightly offset position
     * to avoid overlapping with the original.
     */
    void CloneGameObject(const Entity& entity);
    
    /**
     * @brief Clear all game objects from the world
     * 
     * Destroys all entities in the world and updates the UI cache.
     * Use with caution as this removes all game objects.
     */
    void ClearAllGameObjects();

private:
    GameState m_gameState = GameState::Stopped;
    nlohmann::json m_savedWorldState;

    DebugUIConfig m_config;     ///< Configuration settings for UI layout and appearance
    World* m_world;             ///< Pointer to World object for entity management
    bool m_enabled = false;     ///< Flag indicating if debug UI is currently enabled
    bool m_initialized = false; ///< Flag indicating if ImGui has been initialized

    // UI state
    bool m_showDemo = false;                    ///< Flag to show/hide ImGui demo window
    std::string m_newObjectName = "NewObject"; ///< Default name for new game objects

    // Event counters for input debugging
    int m_spacePressed = 0;   ///< Counter for space key press events
    int m_spaceReleased = 0;  ///< Counter for space key release events

    // Cached UI elements to avoid string creation every frame
    mutable std::unordered_map<EntityId, std::string> m_cachedDeleteLabels;    ///< Cached delete button labels
    mutable std::unordered_map<EntityId, std::string> m_cachedCloneLabels;     ///< Cached clone button labels
    mutable std::unordered_map<EntityId, bool> m_cachedCollapsedHeaders;       ///< Cached header collapse states

    void _showPlayStopControls();
    void _saveWorldState();
    void _restoreWorldState();

    /**
     * @brief Render the main engine debug window
     * 
     * Displays engine status, debug UI state, world connection status,
     * and provides controls for toggling demo window and UI.
     */
    void _showEngineDebugWindow();
    
    /**
     * @brief Render the performance monitoring window
     * 
     * Shows FPS, frame time, memory usage, and other performance metrics
     * for engine optimization and debugging.
     */
    void _showPerformanceWindow();
    
    /**
     * @brief Render the input debugging window
     * 
     * Displays input event counters, key states, mouse position,
     * and other input-related debugging information.
     */
    void _showInputDebugWindow();
    
    /**
     * @brief Render the game object editor window
     * 
     * Provides interface for creating, editing, cloning, and deleting
     * game objects with real-time entity management.
     */
    void _showGameObjectEditor();
    
    /**
     * @brief Render the audio monitoring window
     * @param audio Reference to the audio system
     * 
     * Static method that displays audio system status, volume controls,
     * and audio-related debugging information.
     */
    void _showAudioWindow(Audio::FmodAudioDevice* audio);

    /**
     * @brief Create a new game entity with basic components
     * @param name Name for the new entity
     * @return Entity The created entity with default components
     * 
     * Helper method that creates an entity with transform and other
     * basic components needed for game objects.
     */
    Entity _createGameEntity(const std::string& name);
    
    /**
     * @brief Invalidate UI caches when entities change
     * 
     * Clears cached button labels and states when entities are added,
     * removed, or modified to ensure UI consistency.
     */
    void _invalidateCache();
    
    /**
     * @brief Get cached delete button label for entity
     * @param id Entity ID
     * @return const std::string& Cached delete button label
     */
    const std::string& _getDeleteLabel(EntityId id) const;
    
    /**
     * @brief Get cached clone button label for entity
     * @param id Entity ID
     * @return const std::string& Cached clone button label
     */
    const std::string& _getCloneLabel(EntityId id) const;
    
    /**
     * @brief Get cached header collapse state for entity
     * @param id Entity ID
     * @return const bool& Cached header collapse state
     */
    const bool& _getCollapsedHeaderBool(EntityId id) const;
};

#endif