/**
 * @file DebugUI.h
 * @author Foo Rui Qin
 * @date 2024
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
namespace Audio { class IAudioDevice; }
namespace Scenes { class Scene; }

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
        float EngineW = 300.0f, EngineH = 150.0f;   ///< Engine debug window size

        float InputX = 330.0f, InputY = 10.0f;      ///< Input debug window position
        float InputW = 320.0f, InputH = 400.0f;     ///< Input debug window size

        float EditorX = 670.0f, EditorY = 10.0f;    ///< Game object editor window position
        float EditorW = 375.0f, EditorH = 400.0f;   ///< Game object editor window size

        float PerfX = 1065.0f, PerfY = 10.0f;       ///< Performance monitor window position
        float PerfW = 300.0f, PerfH = 400.0f;       ///< Performance monitor window size
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
 * DebugUI debugUI(&scene);
 * debugUI.Initialize(window);
 * 
 * // In main loop:
 * debugUI.NewFrame();
 * debugUI.Render();
 * @endcode
 */
class DebugUI {
public:
	/**
	 * @brief Constructor for DebugUI
	 * @param config Configuration structure for UI layout and appearance
	 * 
	 * Initializes the debug UI with optional configuration settings.
	 */
    explicit DebugUI(const DebugUIConfig& config = {});

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
    void SetEnabled(const bool enabled) { m_enabled = enabled; }
    
    /**
     * @brief Check if the debug UI is currently enabled
     * @return bool True if UI is enabled, false otherwise
     */
    bool IsEnabled() const { return m_enabled; }

    /**
     * @brief Set the scene reference for entity management
     * @param scene Pointer to the Scene object
     * 
     * Updates the scene reference used for creating and managing entities
     * through the debug interface.
     */
    void AttachScene(Scenes::Scene* scene) { m_scenePtr = scene; }

    /**
     * @brief Detach the current scene reference
     * 
     * Clears the scene pointer, removing the connection to the current
     * scene for entity management.
	 */
	void DetachScene() { m_scenePtr = nullptr; }
    
    /**
     * @brief Check if a valid scene reference exists
     * @return bool True if scene pointer is valid, false otherwise
     */
    bool HasValidScene(const Scenes::Scene* scene = nullptr) const {
		// Check if member scene pointer is valid first
    	if (!m_scenePtr)
            return false;

		// If a specific scene is provided, check for equality
        if (scene)
			return m_scenePtr == scene;

		// Otherwise, just confirm member pointer is valid
	    return m_scenePtr != nullptr;
    }

    /**
     * @brief Attach an audio system for monitoring
     * @param device Pointer to the audio system
     * 
     * Static method to attach an audio system for monitoring and control
     * through the debug interface.
     */
	void AttachAudio(Audio::FmodAudioDevice* device) { m_audioPtr = device; }

    /**
     * @brief Detach the audio system
     * 
     * Static method to detach the audio system from debug monitoring.
     */
	void DetachAudio() { m_audioPtr = nullptr; }

	/**
	 * @brief Check if a valid audio system is attached
	 * 
	 * @return bool True if audio pointer is valid, false otherwise
	 */
	bool HasValidAudio() const { return m_audioPtr != nullptr; }

    /**
     * @brief Add a new game object to the world
     * @param name Name for the new game object
     * 
     * Creates a new entity with basic components and adds it to the world.
     * The object is positioned randomly within the window bounds.
     */
    void AddGameObject(const std::string& name) const;
    
    /**
     * @brief Remove a game object by entity ID
     * @param id Packed Entity ID of the object to remove
     * 
     * Finds and destroys the entity with the specified ID, removing it
     * from the world and updating the UI cache.
     */
    void RemoveGameObject(PackedEntityId id) const;

    /**
     * @brief Clone an existing game object
     * @param entity Entity to clone
     * 
     * Creates a copy of the specified entity with slightly offset position
     * to avoid overlapping with the original.
     */
    void CloneGameObject(const ECS::Entity& entity) const;
    
    /**
     * @brief Clear all game objects from the world
     * 
     * Destroys all entities in the world and updates the UI cache.
     * Use with caution as this removes all game objects.
     */
    void ClearAllGameObjects() const;

private:
    DebugUIConfig m_config;                         ///< Configuration settings for UI layout and appearance
    Scenes::Scene* m_scenePtr = nullptr;            ///< Pointer to Scene object for entity management
	Audio::FmodAudioDevice* m_audioPtr = nullptr;   ///< Pointer to audio system for monitoring
    bool m_enabled = false;                         ///< Flag indicating if debug UI is currently enabled
    bool m_initialized = false;                     ///< Flag indicating if ImGui has been initialized

    // UI state
    bool m_showDemo = false;                    ///< Flag to show/hide ImGui demo window
    std::string m_newObjectName = "NewObject";  ///< Default name for new game objects

    // Event counters for input debugging
    int m_spacePressed = 0;   ///< Counter for space key press events
    int m_spaceReleased = 0;  ///< Counter for space key release events

    // Cached UI elements to avoid string creation every frame
    mutable std::unordered_map<PackedEntityId, std::string> m_cachedDeleteLabels;    ///< Cached delete button labels
    mutable std::unordered_map<PackedEntityId, std::string> m_cachedCloneLabels;     ///< Cached clone button labels
    mutable std::unordered_map<PackedEntityId, bool> m_cachedCollapsedHeaders;       ///< Cached header collapse states

    /**
     * @brief Render the main engine debug window
     * 
     * Displays engine status, debug UI state, scene connection status,
     * and provides controls for toggling demo window and UI.
     */
    void _showEngineDebugWindow();
    
    /**
     * @brief Render the performance monitoring window
     * 
     * Shows FPS, frame time, memory usage, and other performance metrics
     * for engine optimization and debugging.
     */
    void _showPerformanceWindow() const;
    
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
     * @param device Reference to the audio system
     * 
     * Static method that displays audio system status, volume controls,
     * and audio-related debugging information.
     */
    static void _showAudioWindow(Audio::FmodAudioDevice* device);

    /**
     * @brief Create a new game entity with basic components
     * @param name Name for the new entity
     * @return Entity The created entity with default components
     * 
     * Helper method that creates an entity with transform and other
     * basic components needed for game objects.
     */
    ECS::Entity _createGameEntity(const std::string& name) const;
    
    /**
     * @brief Invalidate UI caches when entities change
     * 
     * Clears cached button labels and states when entities are added,
     * removed, or modified to ensure UI consistency.
     */
    void _invalidateCache() const;
    
    /**
     * @brief Get cached delete button label for entity
     * @param id Packed Entity ID
     * @return const std::string& Cached delete button label
     */
    const std::string& _getDeleteLabel(PackedEntityId id) const;
    
    /**
     * @brief Get cached clone button label for entity
     * @param id Packed Entity ID
     * @return const std::string& Cached clone button label
     */
    const std::string& _getCloneLabel(PackedEntityId id) const;
    
    /**
     * @brief Get cached header collapse state for entity
     * @param id Packed Entity ID
     * @return const bool& Cached header collapse state
     */
    const bool& _getCollapsedHeaderBool(PackedEntityId id) const;
};

#endif