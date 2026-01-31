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
#include "services/TimeSystem.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

namespace ECS {

    // Forward declaration to avoid circular dependency
    class ScriptManager;

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
         * @param world Optional World to call OnCreate immediately (nullptr = defer)
         * 
         * METADATA PRIORITY SYSTEM:
         * The system's execution group is resolved using the metadata priority system.
         * C# systems may implement ISystemMetadataProvider via direct casting.
         * 
         * For C# systems, the lifetime is managed by the scripting layer.
         * The system is not owned by SystemManager.
         * 
         * If world is provided, OnCreate() is called immediately on registration.
         * Otherwise, OnCreate() must be called separately via CreateAll().
         */
        void RegisterScriptedSystem(ISystem* system, World* world = nullptr) {
            if (!system) return;

            const auto& metadata = system->GetMetadata();
            SystemGroup group = GetSystemMetadataGroup(system);
            std::string systemName = metadata.GetName();

            m_scriptedSystemGroups[group].push_back(system);
            m_systemsByName[systemName] = system;
            
            // Cache system name to avoid expensive P/Invoke calls during updates
            m_systemNameCache[system] = systemName;

            _sortSystemGroup(group);
            
            // If world is provided, initialize the system immediately
            if (world) {
                system->OnCreate(*world);
            }
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
         * @brief Unregister all scripted systems.
         * @param world World to pass to OnDestroy
         * 
         * Calls OnDestroy() on all scripted systems and removes them from
         * the SystemManager. Used during hot reload to clean up old systems
         * before reloading new ones.
         */
        void UnregisterScriptedSystems(World& world) {
            // Call OnDestroy on all scripted systems
            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    if (system) {
                        system->OnDestroy(world);
                        // Remove from name map
                        const auto& metadata = system->GetMetadata();
                        m_systemsByName.erase(metadata.GetName());

                        // Remove from name cache
                        m_systemNameCache.erase(system);
                    }
                }
            }

            // Clear all scripted system groups
            m_scriptedSystemGroups.clear();
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
         * 
         * Only updates systems whose GetRunMode() matches the specified mode.
         * Used by editor to control which systems run in edit vs play mode.
         * 
         * OPTIMIZATION: Iterates only through system groups that have registered
         * systems, avoiding unnecessary lookups on empty groups. This prevents
         * the O(9*n) lookup pattern that was causing frame stalls in the editor.
         * 
         * Example:
         * @code
         * if (editorInPlayMode) {
         *     systemManager.UpdateSystemsForMode(SystemRunMode::Always, world);
         *     systemManager.UpdateSystemsForMode(SystemRunMode::PlayOnly, world);
         * } else {
         *     systemManager.UpdateSystemsForMode(SystemRunMode::Always, world);
         *     systemManager.UpdateSystemsForMode(SystemRunMode::EditOnly, world);
         * }
         * @endcode
         */
        void UpdateSystemsForMode(SystemRunMode mode, World& world) {
            _updateAllGroupsForMode(mode, world);
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
         * @brief Update all systems in a group (sequential execution).
         * 
         * All systems execute sequentially in order.
         * 
         * @param world Active scene's World
         * @param deltaTime Time since last frame
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
         * @brief Update systems in a group (sequential execution with dependency awareness).
         * 
         * Systems are organized into execution levels based on their component
         * dependencies and execute sequentially within each level.
         * 
         * @param group System group to execute
         * @param world Active scene's World
         * @param deltaTime Time since last frame
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
        // Native C++ systems (owned)
        std::unordered_map<SystemGroup, std::vector<std::unique_ptr<ISystem>>> m_systemGroups;

        // Scripted C# systems (not owned, managed by scripting layer)
        std::unordered_map<SystemGroup, std::vector<ISystem*>> m_scriptedSystemGroups;

        // Name -> System lookup
        std::unordered_map<std::string, ISystem*> m_systemsByName;

        // Cached system names to avoid expensive P/Invoke metadata calls during updates
        // Maps from ISystem* to its cached name string
        std::unordered_map<ISystem*, std::string> m_systemNameCache;

        // Dependency graphs for each system group (for parallel execution analysis)
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
         * @brief Flush buffered logs from C# systems.
         * This ensures all logs accumulated during system execution are delivered
         * to the native side before the next frame.
         * Called from Application after system updates.
         */
        void FlushScriptedLogs() {
            // Implementation is in Application.cpp after UpdateSystemsForMode calls
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
                        // Profile this system's execution
                        TimeSystem::Instance().ProfileBegin(system->GetMetadata().GetName().c_str());
                        system->OnUpdate(world);
                        TimeSystem::Instance().ProfileEnd();
                    }
                }
            }

            // Update scripted systems
            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (system->IsEnabled()) {
                        // Profile this system's execution
                        // Use cached name to avoid expensive P/Invoke metadata lookups
                        auto it = m_systemNameCache.find(system);
                        const char* systemName = (it != m_systemNameCache.end()) ? 
                            it->second.c_str() : "Unknown";
                        TimeSystem::Instance().ProfileBegin(systemName);
                        system->OnUpdate(world);
                        TimeSystem::Instance().ProfileEnd();
                    }
                }
            }
        }

        /**
         * @brief Update all systems filtered by run mode.
         * 
         * Iterates only through system groups that have registered systems,
         * avoiding unnecessary lookups on empty groups.
         */
        void _updateAllGroupsForMode(SystemRunMode mode, World& world) {
            const SystemGroup orderedGroups[] = {
                SystemGroup::PreUpdate,
                SystemGroup::Update,
                SystemGroup::PostUpdate,
                SystemGroup::PrePhysics,
                SystemGroup::Physics,
                SystemGroup::PostPhysics,
                SystemGroup::PreRender,
                SystemGroup::Render,
                SystemGroup::PostRender
            };

            for (SystemGroup group : orderedGroups) {
                _updateGroupForMode(group, mode, world);
            }
        }

        void _updateGroupForMode(SystemGroup group, SystemRunMode mode, World& world) {
            // Update owned systems
            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (system->IsEnabled() && system->GetRunMode() == mode) {
                        TimeSystem::Instance().ProfileBegin(system->GetMetadata().GetName().c_str());
                        system->OnUpdate(world);
                        TimeSystem::Instance().ProfileEnd();
                    }
                }
            }

            // Update scripted systems
            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (system->IsEnabled() && system->GetRunMode() == mode) {
                        auto it = m_systemNameCache.find(system);
                        const char* systemName = (it != m_systemNameCache.end()) ? 
                            it->second.c_str() : "Unknown";
                        TimeSystem::Instance().ProfileBegin(systemName);
                        system->OnUpdate(world);
                        TimeSystem::Instance().ProfileEnd();
                    }
                }
            }
        }

        /**
         * @brief Update systems in a group (sequential execution only).
         */
        void _updateGroupWithJobs(SystemGroup group, World& world) {
            // All systems execute sequentially (job system removed)
            _updateGroup(group, world);
        }

        /**
         * @brief Update a group (dependency-aware sequential execution).
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
                // Execute all systems in this level sequentially
                for (auto* system : level) {
                    if (!system->IsEnabled()) continue;
                    
                    // Use cached name to avoid expensive P/Invoke metadata lookups
                    auto sysIt = m_systemNameCache.find(system);
                    const char* systemName = (sysIt != m_systemNameCache.end()) ? 
                        sysIt->second.c_str() : system->GetMetadata().GetName().c_str();
                    TimeSystem::Instance().ProfileBegin(systemName);
                    system->OnUpdate(world);
                    TimeSystem::Instance().ProfileEnd();
                }
            }
        }
    };

}

#endif
