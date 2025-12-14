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
#include "ecs/jobs/JobHandle.h"
#include "ecs/ComponentAccessAttribute.h"
#include <string>
#include <vector>

namespace ECS {

    /**
     * @brief System run mode for editor play/edit state control
     */
    enum class SystemRunMode {
        Always,         // Runs in both edit and play mode (e.g., Render, Transform hierarchy)
        PlayOnly,       // Only runs in play mode (e.g., Physics, Scripts, AI, Animation)
        EditOnly        // Only runs in edit mode (e.g., Editor gizmos, debug visualization)
    };

    /**
     * @brief Metadata describing a system's component dependencies
     */
    struct SystemMetadata {
        std::string Name;                                   // System name (e.g., "Physics", "Animation")
        std::vector<ComponentTypeId> ReadComponents;        // Components this system reads
        std::vector<ComponentTypeId> WriteComponents;       // Components this system writes
        int ExecutionOrder = 0;                             // Execution priority (lower = earlier)
        bool Enabled = true;                                // Whether system is currently active
    };

    /**
     * @brief System execution group for phased updates
     */
    enum class SystemGroup {
        PreUpdate,      // Input handling, pre-simulation setup
        Update,         // Main gameplay logic (C# systems mostly here)
        PostUpdate,     // Post-gameplay processing
        PrePhysics,     // Physics preparation
        Physics,        // Physics simulation
        PostPhysics,    // Physics response, collision callbacks
        PreRender,      // Frustum culling, LOD selection
        Render,         // Actual rendering
        PostRender      // Cleanup, debug visualization
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
     *     void OnUpdate(World& world, float deltaTime) override {
     *         // Process entities
     *         m_query.Each([deltaTime](Transform& t, Velocity& v) {
     *             t.Position += v.Value * deltaTime;
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
         * @param deltaTime Time elapsed since last frame (in seconds)
         * 
         * This is where all per-frame logic goes. The world parameter
         * represents the currently active scene and may change between calls
         * if scenes are switched.
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
        virtual void OnUpdate(World& world, float deltaTime) = 0;

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
         * Systems are executed in groups to ensure proper ordering:
         * PreUpdate -> Update -> PostUpdate -> PrePhysics -> Physics -> PostPhysics -> PreRender -> Render -> PostRender
         */
        virtual SystemGroup GetSystemGroup() const {
            return SystemGroup::Update; // Default to Update group
        }

        /**
         * @brief Get the system's run mode.
         * @return System run mode (determines if system runs in edit/play/both)
         * 
         * Override to specify when the system should run:
         * - SystemRunMode::Always: Runs in both edit and play mode (e.g., Render, Transform)
         * - SystemRunMode::PlayOnly: Only in play mode (e.g., Physics, Scripts, AI) [DEFAULT]
         * - SystemRunMode::EditOnly: Only in edit mode (e.g., Editor gizmos)
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
            auto accesses = metadata.ReadComponents;
            accesses.insert(accesses.end(), 
                          metadata.WriteComponents.begin(),
                          metadata.WriteComponents.end());
            return accesses;
        }

        /**
         * @brief Execute system as parallel jobs instead of sequential update.
         * 
         * Override to implement job-based system execution. The default
         * implementation calls OnUpdate() sequentially.
         * 
         * @param world Active world
         * @param deltaTime Time since last frame
         * @return JobHandle for tracking execution (default: invalid handle)
         * 
         * Example:
         * @code
         * Jobs::JobHandle OnUpdateAsJobs(World& world, float deltaTime) override {
         *     auto query = world.CreateParallelQuery<Transform, Velocity>();
         *     auto chunks = query.GetChunks();
         *     
         *     std::vector<std::unique_ptr<IJob>> jobs;
         *     for (auto* chunk : chunks) {
         *         auto job = std::make_unique<UpdateChunkJob>();
         *         job->DeltaTime = deltaTime;
         *         jobs.push_back(std::move(job));
         *     }
         *     
         *     return world.GetJobManager().ScheduleParallel(std::move(jobs));
         * }
         * @endcode
         */
        virtual Jobs::JobHandle OnUpdateAsJobs(World& world, float deltaTime) {
            // Default: run sequentially
            OnUpdate(world, deltaTime);
            return Jobs::JobHandle{};
        }

        /**
         * @brief Check if this system supports job-based execution.
         * @return true if OnUpdateAsJobs() is overridden, false if using sequential execution
         */
        virtual bool SupportsJobBasedExecution() const {
            return false;  // Override and return true if implementing OnUpdateAsJobs()
        }

    private:
        bool m_enabled = true;
    };

}

#endif
