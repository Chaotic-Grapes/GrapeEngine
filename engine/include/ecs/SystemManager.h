/* Start Header *****************************************************************/
/*!
\file    SystemManager.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file defines the SystemManager class - responsible for global system registration,
ordering, and execution.

SystemManager replaces per-scene system storage. Systems are registered once at
engine startup and persist across scene transitions. When scenes switch, only
the World they operate on changes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef SYSTEMMANAGER_H
#define SYSTEMMANAGER_H

#include "Export.h"
#include "core/Logger.h"
#include "ecs/ISystem.h"
#include "ecs/ISystemMetadataProvider.h"
#include "ecs/SystemDependencyGraph.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace ECS {

    /**
     * @brief Global manager for all ECS systems.
     * 
     * Responsibilities:
     * - Register systems at engine startup
     * - Sort systems by execution order and group
     * - Execute systems every frame with active scene's World
     * - Enable/disable systems at runtime
     * - Profile system performance
     * 
     * Architecture:
     * - Systems are registered ONCE at engine initialization
     * - Systems persist across scene transitions
     * - Only the World parameter changes when scenes switch
     * - Supports both native C++ and scripted C# systems
     * 
     * Example usage:
     * @code
     * // Engine initialization
     * SystemManager systemManager;
     * 
     * // Register core systems
     * systemManager.RegisterSystem<PhysicsSystem>();
     * systemManager.RegisterSystem<AnimationSystem>();
     * systemManager.RegisterSystem<RenderSystem>();
     * 
     * // Game loop
     * while (running) {
     *     Scene* scene = sceneManager.GetActiveScene();
     *     if (scene) {
     *         systemManager.Update(scene->GetWorld(), deltaTime);
     *     }
     * }
     * 
     * // Shutdown
     * systemManager.DestroyAll();
     * @endcode
     */
    class GRAPEENGINE_API SystemManager {
    public:
        SystemManager() = default;
        ~SystemManager() = default;

        // Delete copy operations (contains unique_ptr)
        SystemManager(const SystemManager&) = delete;
        SystemManager& operator=(const SystemManager&) = delete;

        // Allow move operations
        SystemManager(SystemManager&&) noexcept = default;
        SystemManager& operator=(SystemManager&&) noexcept = default;

        /**
         * @brief Register a new system.
         * @tparam T System type (must inherit from ISystem)
         * @tparam Args Constructor argument types
         * @param args Constructor arguments for the system
         * @return Pointer to the registered system
         * 
         * METADATA PRIORITY SYSTEM:
         * The system's execution group is resolved using this priority order:
         * 1. ISystemMetadataProvider::GetMetadataSystemGroup() (if implemented)
         * 2. ISystem::GetSystemGroup() override
         * 3. Default SystemGroup::Update
         * 
         * This allows the same system class to execute in different groups
         * per instance by implementing ISystemMetadataProvider.
         * 
         * The system is default-constructed, OnCreate() is called, and it's
         * inserted into the execution order based on its metadata.
         * 
         * Example:
         * @code
         * auto* physics = systemManager.RegisterSystem<PhysicsSystem>();
         * auto* audio = systemManager.RegisterSystem<AudioSystem>(audioService);
         * @endcode
         */
        template<typename T, typename... Args>
        T* RegisterSystem(Args&&... args) {
            auto system = std::make_unique<T>(std::forward<Args>(args)...);
            T* ptr = system.get();

            // Get metadata
            const auto& metadata = system->GetMetadata();

            // Store in appropriate group using metadata priority system
            SystemGroup group = GetSystemMetadataGroup(ptr);
            m_systemGroups[group].push_back(std::move(system));

            // Track by name
            m_systemsByName[metadata.GetName()] = ptr;

            // Sort systems by execution order within group
            _sortSystemGroup(group);

            return ptr;
        }

        /**
         * @brief Register a C# scripted system.
         * @param system Raw pointer to C# system wrapper (managed externally)
         * 
         * METADATA PRIORITY SYSTEM:
         * The system's execution group is resolved using the metadata priority system.
         * C# systems may implement ISystemMetadataProvider via direct casting.
         * 
         * For C# systems, the lifetime is managed by the scripting layer.
         * The system is not owned by SystemManager.
         */
        void RegisterScriptedSystem(ISystem* system) {
            if (!system) return;

            const auto& metadata = system->GetMetadata();
            SystemGroup group = GetSystemMetadataGroup(system);

            m_scriptedSystemGroups[group].push_back(system);
            m_systemsByName[metadata.GetName()] = system;

            _sortSystemGroup(group);
        }

        /**
         * @brief Initialize all systems.
         * @param world Initial world (may be empty)
         * 
         * Calls OnCreate() on all registered systems. Typically called once
         * during engine startup after all systems are registered.
         */
        void CreateAll(World& world) {
            for (auto& [group, systems] : m_systemGroups) {
                for (auto& system : systems) {
                    system->OnCreate(world);
                }
            }

            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    system->OnCreate(world);
                }
            }
        }

        /**
         * @brief Update all enabled systems.
         * @param world Active scene's World
         * @param deltaTime Time since last frame (seconds)
         * 
         * Executes systems in group order:
         * PreUpdate -> Update -> PostUpdate -> PrePhysics -> Physics -> PostPhysics -> PreRender -> Render -> PostRender
         * 
         * Within each group, systems are sorted by execution order (lower = earlier).
         */
        void Update(World& world) {
            _updateGroup(SystemGroup::PreUpdate, world);
            _updateGroup(SystemGroup::Update, world);
            _updateGroup(SystemGroup::PostUpdate, world);
            _updateGroup(SystemGroup::PrePhysics, world);
            _updateGroup(SystemGroup::Physics, world);
            _updateGroup(SystemGroup::PostPhysics, world);
            _updateGroup(SystemGroup::PreRender, world);
            _updateGroup(SystemGroup::Render, world);
            _updateGroup(SystemGroup::PostRender, world);
        }

        /**
         * @brief Update systems in a specific group.
         * @param group System group to update
         * @param world Active scene's World
         * @param deltaTime Time since last frame
         */
        void UpdateGroup(SystemGroup group, World& world) {
            _updateGroup(group, world);
        }

        /**
         * @brief Destroy all systems.
         * @param world Last active world
         * 
         * Calls OnDestroy() on all systems. Typically called once during
         * engine shutdown.
         */
        void DestroyAll(World& world) {
            for (auto& [group, systems] : m_systemGroups) {
                for (auto& system : systems) {
                    system->OnDestroy(world);
                }
            }

            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    system->OnDestroy(world);
                }
            }
        }

        /**
         * @brief Get a system by name.
         * @param name System name (from metadata)
         * @return Pointer to system, or nullptr if not found
         */
        ISystem* GetSystem(const std::string& name) {
            auto it = m_systemsByName.find(name);
            return (it != m_systemsByName.end()) ? it->second : nullptr;
        }

        /**
         * @brief Get a system by type.
         * @tparam T System type
         * @return Pointer to system, or nullptr if not found
         */
        template<typename T>
        T* GetSystem() {
            // Search through owned systems
            for (auto& [group, systems] : m_systemGroups) {
                for (auto& system : systems) {
                    if (auto* typed = dynamic_cast<T*>(system.get())) {
                        return typed;
                    }
                }
            }

            // Search through scripted systems
            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    if (auto* typed = dynamic_cast<T*>(system)) {
                        return typed;
                    }
                }
            }

            return nullptr;
        }

        /**
         * @brief Enable or disable a system by name.
         * @param name System name
         * @param enabled New enabled state
         * @return True if system was found, false otherwise
         */
        bool SetSystemEnabled(const std::string& name, bool enabled) {
            ISystem* system = GetSystem(name);
            if (system) {
                system->SetEnabled(enabled);
                return true;
            }
            return false;
        }

        /**
         * @brief Check if a system is enabled.
         * @param name System name
         * @return True if system exists and is enabled, false otherwise
         */
        bool IsSystemEnabled(const std::string& name) const {
            auto it = m_systemsByName.find(name);
            if (it != m_systemsByName.end() && it->second) {
                return it->second->IsEnabled();
            }
            return false;
        }

        /**
         * @brief Update systems based on run mode (for editor play/edit state).
         * @param mode Run mode filter (Always, PlayOnly, or EditOnly)
         * @param world Active scene's World
         * @param deltaTime Time since last frame
         * 
         * Only updates systems whose GetRunMode() matches the specified mode.
         * Used by editor to control which systems run in edit vs play mode.
         * 
         * Example:
         * @code
         * if (editorInPlayMode) {
         *     systemManager.UpdateSystemsForMode(SystemRunMode::Always, world, dt);
         *     systemManager.UpdateSystemsForMode(SystemRunMode::PlayOnly, world, dt);
         * } else {
         *     systemManager.UpdateSystemsForMode(SystemRunMode::Always, world, dt);
         *     systemManager.UpdateSystemsForMode(SystemRunMode::EditOnly, world, dt);
         * }
         * @endcode
         */
        void UpdateSystemsForMode(SystemRunMode mode, World& world) {
            _updateGroupForMode(SystemGroup::PreUpdate, mode, world);
            _updateGroupForMode(SystemGroup::Update, mode, world);
            _updateGroupForMode(SystemGroup::PostUpdate, mode, world);
            _updateGroupForMode(SystemGroup::PrePhysics, mode, world);
            _updateGroupForMode(SystemGroup::Physics, mode, world);
            _updateGroupForMode(SystemGroup::PostPhysics, mode, world);
            _updateGroupForMode(SystemGroup::PreRender, mode, world);
            _updateGroupForMode(SystemGroup::Render, mode, world);
            _updateGroupForMode(SystemGroup::PostRender, mode, world);
        }

        /**
         * @brief Get count of registered systems.
         * @return Total number of systems (native + scripted)
         */
        size_t GetSystemCount() const {
            size_t count = 0;
            for (const auto& [group, systems] : m_systemGroups) {
                count += systems.size();
            }
            for (const auto& [group, systems] : m_scriptedSystemGroups) {
                count += systems.size();
            }
            return count;
        }

        /**
         * @brief Get all registered systems with their execution groups.
         * @return Vector of (name, group) pairs for all systems
         * 
         * Used by editor panels to display system information.
         * Returns systems from both native C++ and scripted C# layers.
         */
        std::vector<std::pair<std::string, SystemGroup>> GetAllSystems() const {
            std::vector<std::pair<std::string, SystemGroup>> result;
            
            for (const auto& [group, systems] : m_systemGroups) {
                for (const auto& system : systems) {
                    result.emplace_back(system->GetMetadata().GetName(), group);
                }
            }
            
            for (const auto& [group, systems] : m_scriptedSystemGroups) {
                for (const auto* system : systems) {
                    result.emplace_back(system->GetMetadata().GetName(), group);
                }
            }
            
            return result;
        }

        /**
         * @brief Check if a system is a scripted (C#) system.
         * @param name System name
         * @return True if the system is a C# scripted system, false if C++ native
         */
        bool IsScriptedSystem(const std::string& name) const {
            // Search scripted systems only
            for (const auto& [group, systems] : m_scriptedSystemGroups) {
                for (const auto* system : systems) {
                    if (system->GetMetadata().GetName() == name) {
                        return true;
                    }
                }
            }
            return false;
        }

        /**
         * @brief Notify all systems that a scene has started playing.
         * Called when transitioning from Edit/Paused mode to Play mode.
         * Allows systems like AudioSystem to perform initialization.
         * @param world The active scene's World
         */
        void OnSceneStart(World& world) {
            (void)world;
            // Call OnSceneStart on all owned systems
            for (auto& [group, systems] : m_systemGroups) {
                for (auto& system : systems) {
                    system->OnSceneStart();
                }
            }

            // Call OnSceneStart on all scripted systems
            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    system->OnSceneStart();
                }
            }
        }

        /**
         * @brief Notify all systems that a scene has stopped playing.
         * Called when transitioning from Play mode to Edit/Paused mode.
         * Allows systems like AudioSystem to stop playback.
         * @param world The active scene's World
         */
        void OnSceneStop(World& world) {
            (void)world;
            // Call OnSceneStop on all owned systems
            for (auto& [group, systems] : m_systemGroups) {
                for (auto& system : systems) {
                    system->OnSceneStop();
                }
            }

            // Call OnSceneStop on all scripted systems
            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    system->OnSceneStop();
                }
            }
        }

        /**
         * @brief Update systems using job-based parallel execution.
         * 
         * Systems that support job-based execution (SupportsJobBasedExecution() == true)
         * are scheduled as jobs and can run in parallel based on component dependencies.
         * Other systems fall back to sequential execution.
         * 
         * @param world Active scene's World
         * @param deltaTime Time since last frame
         * 
         * Note: This experimental feature enables parallel system execution when
         * systems declare their component access patterns correctly.
         */
        void UpdateWithJobs(World& world) {
            _updateGroupWithJobs(SystemGroup::PreUpdate, world);
            _updateGroupWithJobs(SystemGroup::Update, world);
            _updateGroupWithJobs(SystemGroup::PostUpdate, world);
            _updateGroupWithJobs(SystemGroup::PrePhysics, world);
            _updateGroupWithJobs(SystemGroup::Physics, world);
            _updateGroupWithJobs(SystemGroup::PostPhysics, world);
            _updateGroupWithJobs(SystemGroup::PreRender, world);
            _updateGroupWithJobs(SystemGroup::Render, world);
            _updateGroupWithJobs(SystemGroup::PostRender, world);
        }

        /**
         * @brief Update a system group using job-based parallel execution.
         * @param group System group to execute
         * @param world Active scene's World
         * @param deltaTime Time since last frame
         */
        void UpdateGroupWithJobs(SystemGroup group, World& world) {
            _updateGroupWithJobs(group, world);
        }

        /**
         * @brief Rebuild dependency graphs for all system groups.
         * 
         * Analyzes component access patterns and builds dependency graphs
         * for each system group. Call this after registering all systems
         * to enable dependency-aware scheduling.
         * 
         * This enables:
         * - Automatic detection of safe parallel execution
         * - Optimal ordering of independent systems
         * - Component conflict detection
         * 
         * Time complexity: O(g * n^2 * m) where:
         *   g = number of groups
         *   n = systems per group
         *   m = components per system
         */
        void BuildDependencyGraphs() {
            for (auto& [group, graph] : m_dependencyGraphs) {
                graph.Clear();
            }
            m_dependencyGraphs.clear();

            // Build graph for each system group
            for (const auto& [group, systems] : m_systemGroups) {
                auto& graph = m_dependencyGraphs[group];
                
                for (const auto& system : systems) {
                    graph.AddSystem(system.get());
                }
                
                for (const auto* system : m_scriptedSystemGroups[group]) {
                    graph.AddSystem(const_cast<ISystem*>(system));
                }

                graph.Build();

                if (!graph.IsValid()) {
                    // Log warning about cycles in dependency graph
                    LOG_WARNING("SystemManager: Dependency graph for group " << static_cast<int>(group) << " has cycles. Parallel execution may be limited.");
                }
            }
        }

        /**
         * @brief Get the dependency graph for a system group.
         * @param group System group
         * @return Reference to dependency graph (may be empty if not built)
         */
        SystemDependencyGraph& GetDependencyGraph(SystemGroup group) {
            return m_dependencyGraphs[group];
        }

        /**
         * @brief Get the dependency graph for a system group (const).
         * @param group System group
         * @return Const reference to dependency graph
         */
        const SystemDependencyGraph& GetDependencyGraph(SystemGroup group) const {
            auto it = m_dependencyGraphs.find(group);
            if (it != m_dependencyGraphs.end()) {
                return it->second;
            }
            static SystemDependencyGraph empty;
            return empty;
        }

        /**
         * @brief Check if two systems can run in parallel based on dependencies.
         * @param systemA First system
         * @param systemB Second system
         * @return True if they have no component access conflicts
         */
        bool CanSystemsRunInParallel(const ISystem* systemA, const ISystem* systemB) const {
            if (!systemA || !systemB) return false;

            // Use metadata priority system to get execution groups
            SystemGroup groupA = const_cast<ISystem*>(systemA) ? 
                GetSystemMetadataGroup(const_cast<ISystem*>(systemA)) : SystemGroup::Update;
            SystemGroup groupB = const_cast<ISystem*>(systemB) ? 
                GetSystemMetadataGroup(const_cast<ISystem*>(systemB)) : SystemGroup::Update;

            // Can only run in parallel if in same group
            if (groupA != groupB) return false;

            auto it = m_dependencyGraphs.find(groupA);
            if (it != m_dependencyGraphs.end()) {
                return it->second.CanRunInParallel(systemA, systemB);
            }

            return false;
        }

        /**
         * @brief Get execution levels for a system group.
         * 
         * Returns systems organized into levels where all systems in a level
         * can run in parallel, and all levels must complete sequentially.
         * 
         * @param group System group
         * @return Vector of system groups, one per execution level
         */
        std::vector<std::vector<ISystem*>> GetExecutionLevels(SystemGroup group) const {
            auto it = m_dependencyGraphs.find(group);
            if (it != m_dependencyGraphs.end()) {
                return it->second.GetExecutionLevels();
            }
            return {};
        }

        /**
         * @brief Update systems in a group using dependency-aware parallel execution.
         * 
         * Systems are organized into execution levels based on their component
         * dependencies. All systems in a level can run in parallel if they
         * support job-based execution, otherwise they run sequentially.
         * 
         * @param group System group to execute
         * @param world Active scene's World
         * @param deltaTime Time since last frame
         * 
         * This is more sophisticated than UpdateWithJobs - it analyzes actual
         * component access patterns to maximize parallelism safely.
         */
        void UpdateGroupWithDependencies(SystemGroup group, World& world) {
            _updateGroupWithDependencies(group, world);
        }

        /**
         * @brief Update all systems using dependency-aware parallel execution.
         * 
         * @param world Active scene's World
         * @param deltaTime Time since last frame
         */
        void UpdateWithDependencies(World& world) {
            _updateGroupWithDependencies(SystemGroup::PreUpdate, world);
            _updateGroupWithDependencies(SystemGroup::Update, world);
            _updateGroupWithDependencies(SystemGroup::PostUpdate, world);
            _updateGroupWithDependencies(SystemGroup::PrePhysics, world);
            _updateGroupWithDependencies(SystemGroup::Physics, world);
            _updateGroupWithDependencies(SystemGroup::PostPhysics, world);
            _updateGroupWithDependencies(SystemGroup::PreRender, world);
            _updateGroupWithDependencies(SystemGroup::Render, world);
            _updateGroupWithDependencies(SystemGroup::PostRender, world);
        }

    private:
        /// Native C++ systems (owned)
        std::unordered_map<SystemGroup, std::vector<std::unique_ptr<ISystem>>> m_systemGroups;

        /// Scripted C# systems (not owned, managed by scripting layer)
        std::unordered_map<SystemGroup, std::vector<ISystem*>> m_scriptedSystemGroups;

        /// Name -> System lookup
        std::unordered_map<std::string, ISystem*> m_systemsByName;

        /// Dependency graphs for each system group (for parallel execution analysis)
        std::unordered_map<SystemGroup, SystemDependencyGraph> m_dependencyGraphs;

        /**
         * @brief Sort systems within a group by execution order.
         */
        void _sortSystemGroup(SystemGroup group) {
            // Sort owned systems
            auto& systems = m_systemGroups[group];
            std::sort(systems.begin(), systems.end(),
                [](const std::unique_ptr<ISystem>& a, const std::unique_ptr<ISystem>& b) {
                    return a->GetMetadata().GetExecutionOrder() < b->GetMetadata().GetExecutionOrder();
                });

            // Sort scripted systems
            auto& scriptedSystems = m_scriptedSystemGroups[group];
            std::sort(scriptedSystems.begin(), scriptedSystems.end(),
                [](const ISystem* a, const ISystem* b) {
                    return a->GetMetadata().GetExecutionOrder() < b->GetMetadata().GetExecutionOrder();
                });
        }

        /**
         * @brief Update all systems in a specific group.
         */
        void _updateGroup(SystemGroup group, World& world) {
            // Update owned systems
            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (system->IsEnabled()) {
                        system->OnUpdate(world);
                    }
                }
            }

            // Update scripted systems
            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (system->IsEnabled()) {
                        system->OnUpdate(world);
                    }
                }
            }
        }

        /**
         * @brief Update systems in a group filtered by run mode.
         */
        void _updateGroupForMode(SystemGroup group, SystemRunMode mode, World& world) {
            // Update owned systems
            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (system->IsEnabled() && system->GetRunMode() == mode) {
                        system->OnUpdate(world);
                    }
                }
            }

            // Update scripted systems
            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (system->IsEnabled() && system->GetRunMode() == mode) {
                        system->OnUpdate(world);
                    }
                }
            }
        }

        /**
         * @brief Update systems in a group using job-based parallel execution.
         */
        void _updateGroupWithJobs(SystemGroup group, World& world) {
            // Collect all enabled systems in this group that support jobs
            std::vector<ISystem*> jobSystems;
            std::vector<ISystem*> sequentialSystems;

            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (!system->IsEnabled()) continue;
                    
                    if (system->SupportsJobBasedExecution()) {
                        jobSystems.push_back(system.get());
                    }
                    else {
                        sequentialSystems.push_back(system.get());
                    }
                }
            }

            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (!system->IsEnabled()) continue;
                    
                    if (system->SupportsJobBasedExecution()) {
                        jobSystems.push_back(system);
                    }
                    else {
                        sequentialSystems.push_back(system);
                    }
                }
            }

            // Execute sequential systems first (maintain backward compatibility)
            for (auto* system : sequentialSystems) {
                system->OnUpdate(world);
            }

            // Execute job-based systems in parallel (if any)
            if (!jobSystems.empty()) {
                _executeJobSystems(jobSystems, world);
            }
        }

        /**
         * @brief Execute systems using job parallelization.
         */
        void _executeJobSystems(const std::vector<ISystem*>& systems, World& world) {
            /* auto& jobManager = */ world.GetJobManager();

            // Schedule all systems as jobs and wait for completion
            for (auto* system : systems) {
                auto handle = system->OnUpdateAsJobs(world);
                handle.Complete();
            }
        }

        /**
         * @brief Update a group using dependency-aware parallel scheduling.
         */
        void _updateGroupWithDependencies(SystemGroup group, World& world) {
            // Get dependency graph for this group
            auto it = m_dependencyGraphs.find(group);
            if (it == m_dependencyGraphs.end() || it->second.GetSystemCount() == 0) {
                // No dependency info - fall back to sequential update
                _updateGroup(group, world);
                return;
            }

            const auto& graph = it->second;

            // Get execution levels (all systems per level can run in parallel)
            auto levels = graph.GetExecutionLevels();

            for (const auto& level : levels) {
                // Within a level, separate job-based and sequential systems
                std::vector<ISystem*> jobSystems;
                std::vector<ISystem*> sequentialSystems;

                for (auto* system : level) {
                    if (!system->IsEnabled()) continue;
                    
                    if (system->SupportsJobBasedExecution()) {
                        jobSystems.push_back(system);
                    }
                    else {
                        sequentialSystems.push_back(system);
                    }
                }

                // Execute sequential systems first (maintain order)
                for (auto* system : sequentialSystems) {
                    system->OnUpdate(world);
                }

                // Execute job-based systems in parallel
                if (!jobSystems.empty()) {
                    _executeJobSystems(jobSystems, world);
                }
            }
        }
    };

}

#endif
