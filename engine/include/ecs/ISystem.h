/* Start Header *****************************************************************/
/*!
\file    ISystem.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file defines the ISystem interface - the base class for all ECS systems.

All systems (C++ native and C# scripted) must inherit from this interface to
integrate with the SystemManager. Systems have a standardized lifecycle:
- OnCreate: Called once when system is registered
- OnUpdate: Called every frame with active scene's World
- OnDestroy: Called once during engine shutdown

Systems are registered globally at engine startup and persist across scene changes.
Only the World they operate on changes when scenes switch.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef ISYSTEM_H
#define ISYSTEM_H

#include "Export.h"
#include "ecs/World.h"
#include "ecs/SystemEnums.h"
#include "ecs/ComponentAccessAttribute.h"
#include <string>
#include <vector>

// Forward declaration - ISystemMetadataProvider is in a separate header
namespace ECS {
    class ISystemMetadataProvider;
}

namespace ECS {

    /**
     * @brief Metadata describing a system's properties and component dependencies.
     * 
     * Single source of truth for system metadata, consolidating all system information
     * into one structure. IMMUTABLE after creation - prevents inconsistency between
     * component accesses and derived data.
     * 
     * Created exclusively by ComponentAccessBuilder::Build(). All fields are private
     * to enforce immutability. Access is provided through const getter methods.
     * 
     * ComponentAccesses is the authoritative source; Read/Write component lists are
     * computed on-demand for backward compatibility.
     * 
     * @see ComponentAccessBuilder
     */
    class SystemMetadata final {
    public:
        // ====================================================================
        // Const Accessors (No Setters - Immutable)
        // ====================================================================

        /**
         * @brief Get the system name.
         * @return Const reference to system name string
         */
        const std::string& GetName() const { return m_name; }

        /**
         * @brief Get the component access declarations.
         * @return Const reference to component accesses vector
         */
        const std::vector<ComponentAccess>& GetComponentAccesses() const {
            return m_componentAccesses;
        }

        /**
         * @brief Get the execution order priority.
         * @return Execution order (lower = earlier)
         */
        int GetExecutionOrder() const { return m_executionOrder; }

        /**
         * @brief Get the system execution group (phase).
         * @return SystemGroup enum value
         */
        SystemGroup GetGroup() const { return m_group; }

        /**
         * @brief Get when the system runs (edit/play/both).
         * @return SystemRunMode enum value
         */
        SystemRunMode GetRunMode() const { return m_runMode; }

        /**
         * @brief Check if system is enabled.
         * @return True if enabled, false otherwise
         */
        bool IsEnabled() const { return m_enabled; }

        /**
         * @brief Get the system pointer (if available).
         * @return Pointer to ISystem instance, or nullptr if not set
         */
        ISystem* GetSystemPtr() const { return m_systemPtr; }

        /**
         * @brief Get all components this system reads (computed from ComponentAccesses).
         * Provided for backward compatibility with older code.
         * @return Vector of component type IDs with read access
         */
        std::vector<ComponentTypeId> GetReadComponents() const;

        /**
         * @brief Get all components this system writes (computed from ComponentAccesses).
         * Provided for backward compatibility with older code.
         * @return Vector of component type IDs with write access
         */
        std::vector<ComponentTypeId> GetWriteComponents() const;

    private:
        // ====================================================================
        // Private Data (Immutable after construction)
        // ====================================================================

        std::string m_name;
        std::vector<ComponentAccess> m_componentAccesses;
        int m_executionOrder = 0;
        SystemGroup m_group = SystemGroup::Update;
        SystemRunMode m_runMode = SystemRunMode::PlayOnly;
        bool m_enabled = true;
        ISystem* m_systemPtr = nullptr;

        // ====================================================================
        // Private Constructor - Only ComponentAccessBuilder Can Create
        // ====================================================================

        /**
         * @brief Private constructor for immutability.
         * Only ComponentAccessBuilder::Build() can create instances.
         */
        SystemMetadata() = default;

        friend class ComponentAccessBuilder;
    };

    /**
     * @brief Base interface for all ECS systems.
     * 
     * Systems are registered globally with the SystemManager and persist across
     * scene transitions. They operate on the active scene's World.
     * 
     * Lifecycle:
     * 1. OnCreate() - Called once when system is registered (engine startup)
     * 2. OnUpdate() - Called every frame with the active World
     * 3. OnDestroy() - Called once during engine shutdown
     * 
     * Example implementation:
     * @code
     * class MovementSystem : public ISystem {
     * public:
     *     void OnCreate(World& world) override {
     *         // Initialize system state, create queries
     *         m_query = world.CreateQuery<Transform, Velocity>();
     *     }
     *     
    *     void OnUpdate(World& world) override {
    *         // Process entities (use TimeSystem singleton for delta time)
    *         const float dt = static_cast<float>(TimeSystem::Instance().GetDeltaTime());
    *         m_query.Each([dt](Transform& t, Velocity& v) {
    *             t.Position += v.Value * dt;
    *         });
    *     }
     *     
     *     void OnDestroy(World& world) override {
     *         // Cleanup resources
     *     }
     *     
     *     const SystemMetadata& GetMetadata() const override {
     *         static SystemMetadata metadata{
     *             .name = "Movement",
     *             .readComponents = {ComponentTypeId::Get<Velocity>()},
     *             .writeComponents = {ComponentTypeId::Get<Transform>()},
     *             .executionOrder = 10
     *         };
     *         return metadata;
     *     }
     *     
     * private:
     *     Query<Transform, Velocity> m_query;
     * };
     * @endcode
     */
    class GRAPEENGINE_API ISystem {
    public:
        virtual ~ISystem() = default;

        /**
         * @brief Called once when the system is registered.
         * @param world Initial world (may be empty if no scene loaded yet)
         * 
         * Use this to:
         * - Initialize system state
         * - Allocate resources
         * - Create component queries
         * - Register for events
         * 
         * Note: This world reference is NOT guaranteed to be the active world.
         *       Systems should NOT cache world references. Use OnUpdate's world parameter.
         */
        virtual void OnCreate(World& world) { (void)world; }

        /**
         * @brief Called every frame to update the system.
         * @param world The active scene's World
         *
         * Per-frame timing is available via the engine `Time` service/singleton.
         * This avoids passing a delta time parameter to every system. The
         * world parameter represents the currently active scene and may change
         * between calls if scenes are switched.
         *
         * Systems should:
         * - Query components from the provided world
         * - Update entity state
         * - Emit events/messages as needed
         *
         * Systems should NOT:
         * - Cache the world reference
         * - Assume world is the same between calls
         */
        virtual void OnUpdate(World& world) = 0;

        /**
         * @brief Called once during engine shutdown.
         * @param world The last active world (may be empty)
         * 
         * Use this to:
         * - Free allocated resources
         * - Unregister event handlers
         * - Save persistent state
         */
        virtual void OnDestroy(World& world) { (void)world; }

        /**
         * @brief Called when a scene starts playing (editor: transitioning to Play mode).
         * 
         * Use this to:
         * - Initialize PlayOnly-specific state
         * - Start audio playback
         * - Reset physics state
         * 
         * Default: no-op (override if needed)
         */
        virtual void OnSceneStart() {}

        /**
         * @brief Called when a scene stops playing (editor: transitioning away from Play mode).
         * 
         * Use this to:
         * - Stop audio playback
         * - Clean up PlayOnly-specific state
         * - Pause simulations
         * 
         * Default: no-op (override if needed)
         */
        virtual void OnSceneStop() {}

        /**
         * @brief Determine whether this system should run on this frame.
         * @param world The active scene's World
         * @return True if OnUpdate should execute, false to skip this frame
         *
         * Use this for lightweight run gating (DOTS-style "has work to do").
         * The default implementation always runs.
         */
        virtual bool ShouldRun(World& world) { (void)world; return true; }

        /**
         * @brief Indicates whether this system requires a per-frame ShouldRun evaluation.
         * @return True when SystemManager must call ShouldRun() each frame, false when
         *         the system is always runnable while enabled and mode-eligible.
         * @note Default is true to preserve existing behavior for native systems.
         */
        virtual bool RequiresShouldRunCheck() const { return true; }

        /**
         * @brief Called when the system transitions from not-running to running.
         * @param world The active scene's World
         *
         * This can fire multiple times during a system's lifetime.
         * Default: no-op.
         */
        virtual void OnStartRunning(World& world) { (void)world; }

        /**
         * @brief Called when the system transitions from running to not-running.
         * @param world The active scene's World
         *
         * This can fire multiple times during a system's lifetime.
         * Default: no-op.
         */
        virtual void OnStopRunning(World& world) { (void)world; }

        /**
         * @brief Get system metadata (name, dependencies, execution order).
         * @return SystemMetadata by value
         * 
         * This is queried by the SystemManager to:
         * - Sort systems by execution order
         * - Detect read/write conflicts
         * - Display system information in profiler
         */
        virtual SystemMetadata GetMetadata() const = 0;

        /**
         * @brief Get the system group this system belongs to.
         * @return SystemGroup enum value
         * 
         * METADATA PRIORITY SYSTEM:
         * This method is part of a two-level priority system for resolving metadata:
         * - Priority 1: ISystemMetadataProvider interface (if implemented)
         * - Priority 2: This GetSystemGroup() override (this method)
         * - Priority 3: Default SystemGroup::Update
         * 
         * If your system needs RUNTIME configuration of execution group (same class,
         * different group per instance), implement ISystemMetadataProvider instead.
         * 
         * Systems are executed in groups to ensure proper ordering:
         * PreUpdate -> Update -> PostUpdate -> PrePhysics -> Physics -> PostPhysics -> PreRender -> Render -> PostRender
         * 
         * @see ISystemMetadataProvider for runtime metadata flexibility
         * @see GetSystemMetadataGroup() for priority-aware group resolution
         */
        virtual SystemGroup GetSystemGroup() const {
            return SystemGroup::Update; // Default to Update group
        }

        /**
         * @brief Get the system's run mode.
         * @return System run mode (determines if system runs in edit/play/both)
         * 
         * METADATA PRIORITY SYSTEM:
         * This method is part of a two-level priority system for resolving metadata:
         * - Priority 1: ISystemMetadataProvider interface (if implemented)
         * - Priority 2: This GetRunMode() override (this method)
         * - Priority 3: Default SystemRunMode::PlayOnly
         * 
         * If your system needs RUNTIME configuration of run mode (same class,
         * different run mode per instance), implement ISystemMetadataProvider instead.
         * 
         * Override to specify when the system should run:
         * - SystemRunMode::Always: Runs in both edit and play mode (e.g., Render, Transform)
         * - SystemRunMode::PlayOnly: Only in play mode (e.g., Physics, Scripts, AI) [DEFAULT]
         * - SystemRunMode::EditOnly: Only in edit mode (e.g., Editor gizmos)
         * 
         * @see ISystemMetadataProvider for runtime metadata flexibility
         * @see GetSystemMetadataRunMode() for priority-aware run mode resolution
         */
        virtual SystemRunMode GetRunMode() const { return SystemRunMode::PlayOnly; }

        /**
         * @brief Check if this system is currently enabled.
         * @return True if enabled, false otherwise
         * 
         * Disabled systems skip OnUpdate() but remain registered.
         * Can be toggled at runtime via SystemManager.
         */
        bool IsEnabled() const { return m_enabled; }

        /**
         * @brief Enable or disable this system.
         * @param enabled New enabled state
         */
        void SetEnabled(bool enabled) { m_enabled = enabled; }

        /**
         * @brief Get component access patterns for dependency resolution.
         * 
         * Override to declare which components this system reads/writes.
         * Used by SystemManager for automatic dependency detection.
         * 
         * @return Vector of component type IDs (read and write combined from metadata)
         * 
         * Example:
         * @code
         * std::vector<ComponentTypeId> GetComponentAccesses() const override {
         *     return {
         *         TypeIdOf<Transform>(),   // Read
         *         TypeIdOf<Velocity>(),    // Read
         *         // Writes are in WriteComponents from metadata
         *     };
         * }
         * @endcode
         */
        virtual std::vector<ComponentTypeId> GetComponentAccesses() const {
            // Default: combine read and write components from metadata
            auto metadata = GetMetadata();
            auto accesses = metadata.GetReadComponents();
            auto writes = metadata.GetWriteComponents();
            accesses.insert(accesses.end(), 
                          writes.begin(),
                          writes.end());
            return accesses;
        }


    private:
        bool m_enabled = true;
    };

}

#endif
