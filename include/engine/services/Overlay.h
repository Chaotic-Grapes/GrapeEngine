/**
 * @file Overlay.h
 * @author Foo Rui Qin
 * @date 2024
 * @brief Overlay system for managing debug UI and ImGui integration
 * 
 * This file defines the Overlay class which serves as a system-level wrapper
 * for the debug UI functionality. The Overlay system manages:
 * - ImGui initialization and integration with the engine
 * - Debug UI lifecycle management (creation, updates, cleanup)
 * - Audio system integration for debug monitoring
 * - Conditional compilation support for ImGui features
 * - Window management integration for UI rendering
 * - World reference management for entity debugging
 * 
 * The Overlay system inherits from Engine::ISystem and follows the ECS pattern,
 * providing a clean interface between the engine's system architecture and
 * the ImGui-based debug interface.
 */

#ifndef OVERLAY_H
#define OVERLAY_H
#include <memory>
#include "ecs/ISystem.h"
#include "audio/Audio.h"
#include "services/DebugUI.h"
#include "core/Application.h"

// Forward declaration
class World;
#ifdef USE_IMGUI
class DebugUI;
#endif

/**
 * @brief Overlay system for managing debug UI and ImGui integration
 * 
 * The Overlay class serves as a system-level wrapper that manages the debug UI
 * functionality within the engine's ECS architecture. It handles the lifecycle
 * of ImGui integration, debug UI creation and updates, and provides a bridge
 * between the engine systems and the debug interface.
 * 
 * Key responsibilities:
 * - Initialize and manage ImGui context and backends
 * - Create and manage DebugUI instance lifecycle
 * - Handle audio system integration for debug monitoring
 * - Manage window references for UI rendering
 * - Provide conditional compilation support for ImGui features
 * - Ensure proper cleanup and resource management
 * 
 * The system follows the Engine::ISystem interface pattern and integrates
 * seamlessly with the engine's update loop and system management.
 * 
 * Usage example:
 * @code
 * auto overlay = std::make_unique<Overlay>(&world);
 * overlay->SetAudio(&audioSystem);
 * systemManager.AddSystem(std::move(overlay));
 * @endcode
 */
class Overlay final : public Engine::ISystem {
public:
    /**
     * @brief Constructor for Overlay system
     * @param world Pointer to the World object for entity management
     * 
     * Initializes the overlay system with a reference to the world.
     * The world pointer is used to create and manage the debug UI
     * which needs access to entities for debugging purposes.
     */
    explicit Overlay(World* world) : m_world(world) {}
    
    /**
     * @brief Initialize the overlay system
     * 
     * Called once when the system is created. Creates the DebugUI instance
     * if a valid world reference exists. This method is part of the
     * Engine::ISystem interface and is called by the system manager.
     */
    void OnCreate() override;
    
    /**
     * @brief Update the overlay system each frame
     * 
     * Called every frame to update the overlay system. Handles:
     * - DebugUI instance creation if not already created
     * - ImGui initialization when window becomes available
     * - Audio system attachment for debug monitoring
     * - Frame-by-frame UI rendering and updates
     * 
     * This method manages the complete UI update cycle including
     * ImGui frame setup, rendering, and finalization.
     */
    void OnUpdate() override;

#ifdef USE_IMGUI
    /**
     * @brief Destructor for Overlay system
     * 
     * Properly shuts down the debug UI and detaches the audio system
     * to prevent memory leaks and ensure clean resource cleanup.
     * Only defined when ImGui is available to avoid linking issues.
     */
    ~Overlay() override;
#endif

    /**
     * @brief Get the name of the system
     * @return std::string The name "Overlay" for debugging and logging
     * 
     * Returns the system name for identification in logs, debugging,
     * and system management. Part of the Engine::ISystem interface.
     */
    std::string Name() const override { return "Overlay"; }
    
    /**
     * @brief Set the audio system for debug monitoring
     * @param a Pointer to the audio system to monitor
     * 
     * Attaches an audio system to the overlay for debug monitoring.
     * The audio system will be accessible through the debug UI for
     * real-time monitoring and control.
     */
    void SetAudio(Systems::Audio* a) { m_audio = a; }

private:
    Systems::Audio* m_audio = nullptr;  ///< Pointer to audio system for debug monitoring
    
    /**
     * @brief Set the world reference for entity management
     * @param world Pointer to the World object
     * 
     * Private setter method to update the world reference used by
     * the debug UI for entity management and debugging.
     */
    void SetWorld(World* world) { m_world = world; }
    
    World* m_world = nullptr;  ///< Pointer to World object for entity management

#ifdef USE_IMGUI
    std::unique_ptr<DebugUI> m_debugUI;  ///< Unique pointer to DebugUI instance for memory management
    bool m_initialized = false;          ///< Flag indicating if ImGui has been initialized
#endif
};

#endif
