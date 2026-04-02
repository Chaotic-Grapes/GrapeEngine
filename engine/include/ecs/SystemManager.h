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
#include <unordered_set>
#include <string>

#ifndef GRAPE_ENABLE_PROFILING
#define GRAPE_ENABLE_PROFILING 1
#endif

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

            // Cache resolved run mode to avoid per-frame RTTI/dynamic_cast work.
            m_systemRunModeCache[ptr] = GetSystemMetadataRunMode(ptr);

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
            m_systemRunModeCache[system] = GetSystemMetadataRunMode(system);
            
            // Cache system name to avoid expensive P/Invoke calls during updates
            m_systemNameCache[system] = systemName;

            _sortSystemGroup(group);
            
            // If world is provided and run mode is active, initialize immediately
            if (world && IsRunModeActive(_getCachedRunMode(system))) {
                system->OnCreate(*world);
                m_createdSystems.insert(system);
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
                    if (!IsSystemCreated(system.get())) {
                        system->OnCreate(world);
                        m_createdSystems.insert(system.get());
                    }
                }
            }

            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    if (!IsSystemCreated(system)) {
                        system->OnCreate(world);
                        m_createdSystems.insert(system);
                    }
                }
            }
        }

        /**
         * @brief Initialize all systems that match a run mode.
         * @param mode Run mode to create
         * @param world Initial world
         */
        void CreateSystemsForMode(SystemRunMode mode, World& world) {
            _createAllGroupsForMode(mode, world);
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
            _applyPendingEnabledChanges();
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
            _applyPendingEnabledChanges();
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
                    _stopIfRunning(system.get(), world);
                    system->OnDestroy(world);
                }
            }

            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    _stopIfRunning(system, world);
                    system->OnDestroy(world);
                }
            }

            m_createdSystems.clear();
            m_runningSystems.clear();
            m_pendingEnabledByName.clear();
        }

        /**
         * @brief Destroy all systems that match a run mode.
         * @param mode Run mode to destroy
         * @param world Active world
         */
        void DestroySystemsForMode(SystemRunMode mode, World& world) {
            _destroyAllGroupsForMode(mode, world);
        }

        /**
         * @brief Set the active run mode bitmask (used for registration-time OnCreate).
         * @param modes Bitmask of SystemRunMode values to treat as active
         */
        void SetActiveRunModeMask(uint32_t modes) {
            m_activeRunModesMask = modes;
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
                        _stopIfRunning(system, world);
                        system->OnDestroy(world);
                        // Remove from name map
                        const auto& metadata = system->GetMetadata();
                        m_systemsByName.erase(metadata.GetName());

                        // Remove from name cache
                        m_systemNameCache.erase(system);

                        // Remove from created set
                        m_createdSystems.erase(system);
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
                m_pendingEnabledByName[name] = enabled;
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
            _applyPendingEnabledChanges();
            _updateAllGroupsForMode(mode, world);
        }

        /**
         * @brief Update systems for all active run modes in one traversal.
         * @param modeMask Bitmask of active run modes.
         * @param world Active scene world.
         * @note This avoids traversing all systems once per mode when multiple
         *       modes are active in the same frame (for example Always + PlayOnly).
         * @complexity O(g + n), where g is group count and n is total systems.
         */
        void UpdateSystemsByMask(uint32_t modeMask, World& world) {
            _applyPendingEnabledChanges();
            _updateAllGroupsForMask(modeMask, world);
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
                    if (!IsSystemCreated(system.get()))
                        continue;
                    auto runMode = _getCachedRunMode(system.get());
                    if (runMode == SystemRunMode::EditOnly)
                        continue;
                    system->OnSceneStart();
                }
            }

            // Call OnSceneStart on all scripted systems
            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    if (!IsSystemCreated(system))
                        continue;
                    auto runMode = _getCachedRunMode(system);
                    if (runMode == SystemRunMode::EditOnly)
                        continue;
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
                    if (!IsSystemCreated(system.get()))
                        continue;
                    auto runMode = _getCachedRunMode(system.get());
                    if (runMode == SystemRunMode::EditOnly)
                        continue;
                    system->OnSceneStop();
                }
            }

            // Call OnSceneStop on all scripted systems
            for (auto& [group, systems] : m_scriptedSystemGroups) {
                for (auto* system : systems) {
                    if (!IsSystemCreated(system))
                        continue;
                    auto runMode = _getCachedRunMode(system);
                    if (runMode == SystemRunMode::EditOnly)
                        continue;
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
            _applyPendingEnabledChanges();
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
            _applyPendingEnabledChanges();
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
            _applyPendingEnabledChanges();
            _updateGroupWithDependencies(group, world);
        }

        /**
         * @brief Update all systems using dependency-aware parallel execution.
         * 
         * @param world Active scene's World
         * @param deltaTime Time since last frame
         */
        void UpdateWithDependencies(World& world) {
            _applyPendingEnabledChanges();
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
        /**
         * @brief Begin an ECS profiler scope when compile-time profiling is enabled.
         * @param scopeName Scope name string literal or stable c-string.
         * @note Compiles to a no-op for export builds where profiling is stripped.
         * @complexity O(1).
         */
        static void _profileBegin(const char* scopeName) {
    #if GRAPE_ENABLE_PROFILING
            TimeSystem::Instance().ProfileBegin(scopeName);
    #else
            (void)scopeName;
    #endif
        }

        /**
         * @brief End an ECS profiler scope when compile-time profiling is enabled.
         * @note Compiles to a no-op for export builds where profiling is stripped.
         * @complexity O(1).
         */
        static void _profileEnd() {
    #if GRAPE_ENABLE_PROFILING
            TimeSystem::Instance().ProfileEnd();
    #endif
        }

        // Native C++ systems (owned)
        std::unordered_map<SystemGroup, std::vector<std::unique_ptr<ISystem>>> m_systemGroups;

        // Scripted C# systems (not owned, managed by scripting layer)
        std::unordered_map<SystemGroup, std::vector<ISystem*>> m_scriptedSystemGroups;

        // Name -> System lookup
        std::unordered_map<std::string, ISystem*> m_systemsByName;

        // Cached system names to avoid expensive P/Invoke metadata calls during updates
        // Maps from ISystem* to its cached name string
        std::unordered_map<ISystem*, std::string> m_systemNameCache;

        // Cached run mode for each system to avoid per-frame metadata RTTI lookups.
        std::unordered_map<ISystem*, SystemRunMode> m_systemRunModeCache;

        // Dependency graphs for each system group (for parallel execution analysis)
        std::unordered_map<SystemGroup, SystemDependencyGraph> m_dependencyGraphs;

        // Track created systems to avoid duplicate OnCreate calls
        std::unordered_set<ISystem*> m_createdSystems;

        // Track systems that are currently in the "running" state.
        std::unordered_set<ISystem*> m_runningSystems;

        // Deferred enabled-state writes applied at update frame boundaries.
        std::unordered_map<std::string, bool> m_pendingEnabledByName;

        // Active run mode bitmask (used for registration-time OnCreate)
        uint32_t m_activeRunModesMask =
            (1u << static_cast<uint32_t>(SystemRunMode::Always)) |
            (1u << static_cast<uint32_t>(SystemRunMode::PlayOnly));

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
#if GRAPE_ENABLE_PROFILING
                    _updateSystemWithRunState(system.get(), world, system->GetMetadata().GetName().c_str(), true);
#else
                    _updateSystemWithRunState(system.get(), world, nullptr, true);
#endif
                }
            }

            // Update scripted systems
            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
#if GRAPE_ENABLE_PROFILING
                    // Use cached name to avoid expensive P/Invoke metadata lookups.
                    auto it = m_systemNameCache.find(system);
                    const char* systemName = (it != m_systemNameCache.end()) ?
                        it->second.c_str() : "Unknown";
                    _updateSystemWithRunState(system, world, systemName, true);
#else
                    _updateSystemWithRunState(system, world, nullptr, true);
#endif
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
                // Profile each run-mode/group bucket separately so editor tooling can
                // pinpoint hot phases inside UpdateSystemsByMode without API changes.
                const char* scopeName = _getModeGroupProfileScopeName(mode, group);
                _profileBegin(scopeName);
                _updateGroupForMode(group, mode, world);
                _profileEnd();
            }
        }

        /**
         * @brief Update all groups for a run-mode bitmask in a single pass.
         * @param modeMask Active run mode bitmask.
         * @param world Active world for updates.
         * @complexity O(g + n), where g is group count and n is total systems.
         */
        void _updateAllGroupsForMask(uint32_t modeMask, World& world) {
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
                _profileBegin(_getMaskGroupProfileScopeName(group));
                _updateGroupForMask(group, modeMask, world);
                _profileEnd();
            }
        }

        /**
         * @brief Resolve profiler scope name for mask-based group updates.
         * @param group System group currently being updated.
         * @return Stable string literal scope name.
         * @complexity O(1).
         */
        static const char* _getMaskGroupProfileScopeName(SystemGroup group) {
            switch (group) {
            case SystemGroup::PreUpdate:  return "ECS.Mask.PreUpdate";
            case SystemGroup::Update:     return "ECS.Mask.Update";
            case SystemGroup::PostUpdate: return "ECS.Mask.PostUpdate";
            case SystemGroup::PrePhysics: return "ECS.Mask.PrePhysics";
            case SystemGroup::Physics:    return "ECS.Mask.Physics";
            case SystemGroup::PostPhysics:return "ECS.Mask.PostPhysics";
            case SystemGroup::PreRender:  return "ECS.Mask.PreRender";
            case SystemGroup::Render:     return "ECS.Mask.Render";
            case SystemGroup::PostRender: return "ECS.Mask.PostRender";
            }
            return "ECS.Mask.UnknownGroup";
        }

        /**
         * @brief Check whether a run mode is enabled in a bitmask.
         * @param mode Run mode to test.
         * @param modeMask Active mode mask.
         * @return True when the mode bit is set.
         * @complexity O(1).
         */
        static bool _isModeEnabledInMask(SystemRunMode mode, uint32_t modeMask) {
            const uint32_t bit = 1u << static_cast<uint32_t>(mode);
            return (modeMask & bit) != 0u;
        }

        /**
         * @brief Resolve a stable profiler scope name for a mode/group execution bucket.
         * @param mode Active run mode being executed.
         * @param group System group currently being processed.
         * @return Interned string literal suitable for per-frame profiling.
         * @complexity O(1).
         */
        static const char* _getModeGroupProfileScopeName(SystemRunMode mode, SystemGroup group) {
            switch (mode) {
            case SystemRunMode::Always:
                switch (group) {
                case SystemGroup::PreUpdate:  return "ECS.Always.PreUpdate";
                case SystemGroup::Update:     return "ECS.Always.Update";
                case SystemGroup::PostUpdate: return "ECS.Always.PostUpdate";
                case SystemGroup::PrePhysics: return "ECS.Always.PrePhysics";
                case SystemGroup::Physics:    return "ECS.Always.Physics";
                case SystemGroup::PostPhysics:return "ECS.Always.PostPhysics";
                case SystemGroup::PreRender:  return "ECS.Always.PreRender";
                case SystemGroup::Render:     return "ECS.Always.Render";
                case SystemGroup::PostRender: return "ECS.Always.PostRender";
                }
                break;
            case SystemRunMode::PlayOnly:
                switch (group) {
                case SystemGroup::PreUpdate:  return "ECS.PlayOnly.PreUpdate";
                case SystemGroup::Update:     return "ECS.PlayOnly.Update";
                case SystemGroup::PostUpdate: return "ECS.PlayOnly.PostUpdate";
                case SystemGroup::PrePhysics: return "ECS.PlayOnly.PrePhysics";
                case SystemGroup::Physics:    return "ECS.PlayOnly.Physics";
                case SystemGroup::PostPhysics:return "ECS.PlayOnly.PostPhysics";
                case SystemGroup::PreRender:  return "ECS.PlayOnly.PreRender";
                case SystemGroup::Render:     return "ECS.PlayOnly.Render";
                case SystemGroup::PostRender: return "ECS.PlayOnly.PostRender";
                }
                break;
            case SystemRunMode::EditOnly:
                switch (group) {
                case SystemGroup::PreUpdate:  return "ECS.EditOnly.PreUpdate";
                case SystemGroup::Update:     return "ECS.EditOnly.Update";
                case SystemGroup::PostUpdate: return "ECS.EditOnly.PostUpdate";
                case SystemGroup::PrePhysics: return "ECS.EditOnly.PrePhysics";
                case SystemGroup::Physics:    return "ECS.EditOnly.Physics";
                case SystemGroup::PostPhysics:return "ECS.EditOnly.PostPhysics";
                case SystemGroup::PreRender:  return "ECS.EditOnly.PreRender";
                case SystemGroup::Render:     return "ECS.EditOnly.Render";
                case SystemGroup::PostRender: return "ECS.EditOnly.PostRender";
                }
                break;
            }

            return "ECS.UnknownGroup";
        }

        bool IsRunModeActive(SystemRunMode mode) const {
            auto bit = 1u << static_cast<uint32_t>(mode);
            return (m_activeRunModesMask & bit) != 0;
        }

        bool IsSystemCreated(const ISystem* system) const {
            return system && (m_createdSystems.find(const_cast<ISystem*>(system)) != m_createdSystems.end());
        }

        bool IsSystemRunning(const ISystem* system) const {
            return system && (m_runningSystems.find(const_cast<ISystem*>(system)) != m_runningSystems.end());
        }

        /**
         * @brief Resolve a system's run mode from cache with safe fallback.
         * @param system System pointer.
         * @return Cached run mode, or metadata-resolved mode when cache is missing.
         * @complexity Average O(1).
         */
        SystemRunMode _getCachedRunMode(ISystem* system) const {
            auto it = m_systemRunModeCache.find(system);
            if (it != m_systemRunModeCache.end()) {
                return it->second;
            }
            return GetSystemMetadataRunMode(system);
        }

        void _stopIfRunning(ISystem* system, World& world) {
            if (!IsSystemRunning(system)) {
                return;
            }
            system->OnStopRunning(world);
            m_runningSystems.erase(system);
        }

        void _applyPendingEnabledChanges() {
            if (m_pendingEnabledByName.empty()) {
                return;
            }

            // Apply all pending enabled state changes
            for (const auto& [name, enabled] : m_pendingEnabledByName) {
                auto it = m_systemsByName.find(name);

                if (it == m_systemsByName.end() || !it->second) {
                    continue;
                }
                it->second->SetEnabled(enabled);
            }
            m_pendingEnabledByName.clear();
        }

        /**
         * @brief Update one system while honoring creation, run-state transitions, and eligibility.
         * @param system System instance to evaluate and potentially update.
         * @param world Active world passed into lifecycle/update callbacks.
         * @param profileName Profiling scope name used for update timing.
         * @param runModeEligible True when caller has already validated run-mode eligibility.
         * @note Start/stop transitions are emitted exactly once per state edge.
         * @complexity O(1) excluding the system's own callback work.
         */
        void _updateSystemWithRunState(ISystem* system, World& world, const char* profileName, bool runModeEligible) {
            if (!system) {
                return;
            }
            if (!IsSystemCreated(system)) {
                m_runningSystems.erase(system);
                return;
            }

            // Determine if the system should be running based on its enabled state and run mode eligibility
            const bool isRunning = IsSystemRunning(system);
            const bool shouldRunNow = system->IsEnabled() &&
                runModeEligible &&
                (!system->RequiresShouldRunCheck() || system->ShouldRun(world));

            // Handle state transitions for starting/stopping systems
            if (!isRunning && shouldRunNow) {
                system->OnStartRunning(world);
                m_runningSystems.insert(system);
            } else if (isRunning && !shouldRunNow) {
                system->OnStopRunning(world);
                m_runningSystems.erase(system);
                return;
            } else if (!shouldRunNow) {
                return;
            }

            _profileBegin(profileName);
            system->OnUpdate(world);
            _profileEnd();
        }

        void _createAllGroupsForMode(SystemRunMode mode, World& world) {
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
                _createGroupForMode(group, mode, world);
            }
        }

        void _destroyAllGroupsForMode(SystemRunMode mode, World& world) {
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
                _destroyGroupForMode(group, mode, world);
            }
        }

        /**
         * @brief Create systems in a group that match the requested run mode.
         * @param group System group bucket.
         * @param mode Run mode to create.
         * @param world Active world passed to OnCreate.
         * @complexity O(n) for systems in the group.
         */
        void _createGroupForMode(SystemGroup group, SystemRunMode mode, World& world) {
            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (_getCachedRunMode(system.get()) != mode)
                        continue;
                    if (IsSystemCreated(system.get()))
                        continue;
                    system->OnCreate(world);
                    m_createdSystems.insert(system.get());
                }
            }

            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (_getCachedRunMode(system) != mode)
                        continue;
                    if (IsSystemCreated(system))
                        continue;
                    system->OnCreate(world);
                    m_createdSystems.insert(system);
                }
            }
        }

        /**
         * @brief Destroy systems in a group that match the requested run mode.
         * @param group System group bucket.
         * @param mode Run mode to destroy.
         * @param world Active world passed to OnDestroy.
         * @complexity O(n) for systems in the group.
         */
        void _destroyGroupForMode(SystemGroup group, SystemRunMode mode, World& world) {
            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (_getCachedRunMode(system.get()) != mode)
                        continue;
                    if (!IsSystemCreated(system.get()))
                        continue;
                    _stopIfRunning(system.get(), world);
                    system->OnDestroy(world);
                    m_createdSystems.erase(system.get());
                }
            }

            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (_getCachedRunMode(system) != mode)
                        continue;
                    if (!IsSystemCreated(system))
                        continue;
                    _stopIfRunning(system, world);
                    system->OnDestroy(world);
                    m_createdSystems.erase(system);
                }
            }
        }

        /**
         * @brief Update systems in a group for a single run mode.
         * @param group System group bucket.
         * @param mode Run mode filter.
         * @param world Active world for update.
         * @note Systems with non-matching run mode are skipped early to avoid
         *       run-state checks and per-system overhead.
         * @complexity O(n) for systems in the group.
         */
        void _updateGroupForMode(SystemGroup group, SystemRunMode mode, World& world) {
            // Update owned systems
            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (_getCachedRunMode(system.get()) != mode) {
                        continue;
                    }
#if GRAPE_ENABLE_PROFILING
                    _updateSystemWithRunState(system.get(), world, system->GetMetadata().GetName().c_str(), true);
#else
                    _updateSystemWithRunState(system.get(), world, nullptr, true);
#endif
                }
            }

            // Update scripted systems
            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (_getCachedRunMode(system) != mode) {
                        continue;
                    }
#if GRAPE_ENABLE_PROFILING
                    auto it = m_systemNameCache.find(system);
                    const char* systemName = (it != m_systemNameCache.end()) ?
                        it->second.c_str() : "Unknown";
                    _updateSystemWithRunState(system, world, systemName, true);
#else
                    _updateSystemWithRunState(system, world, nullptr, true);
#endif
                }
            }
        }

        /**
         * @brief Update systems in a group using a run-mode bitmask.
         * @param group System group bucket.
         * @param modeMask Active run-mode bitmask.
         * @param world Active world for update.
         * @complexity O(n) for systems in the group.
         */
        void _updateGroupForMask(SystemGroup group, uint32_t modeMask, World& world) {
            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (!_isModeEnabledInMask(_getCachedRunMode(system.get()), modeMask)) {
                        continue;
                    }
#if GRAPE_ENABLE_PROFILING
                    _updateSystemWithRunState(system.get(), world, system->GetMetadata().GetName().c_str(), true);
#else
                    _updateSystemWithRunState(system.get(), world, nullptr, true);
#endif
                }
            }

            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (!_isModeEnabledInMask(_getCachedRunMode(system), modeMask)) {
                        continue;
                    }
#if GRAPE_ENABLE_PROFILING
                    auto itName = m_systemNameCache.find(system);
                    const char* systemName = (itName != m_systemNameCache.end()) ?
                        itName->second.c_str() : "Unknown";
                    _updateSystemWithRunState(system, world, systemName, true);
#else
                    _updateSystemWithRunState(system, world, nullptr, true);
#endif
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
            // Helper lambda to update a single system with checks and profiling
            auto updateSystem = [&](ISystem* system) {
                if (!system) {
                    return;
                }
#if GRAPE_ENABLE_PROFILING
                // Use cached name to avoid expensive P/Invoke metadata lookups.
                auto sysIt = m_systemNameCache.find(system);
                const char* systemName = (sysIt != m_systemNameCache.end()) ?
                    sysIt->second.c_str() : system->GetMetadata().GetName().c_str();
#else
                const char* systemName = nullptr;
#endif
                const bool eligible = IsRunModeActive(_getCachedRunMode(system));
                _updateSystemWithRunState(system, world, systemName, eligible);
            };

            // Get dependency graph for this group
            auto it = m_dependencyGraphs.find(group);
            if (it == m_dependencyGraphs.end() || it->second.GetSystemCount() == 0) {
                // No dependency info - fall back to sequential update with run-mode filtering.
                auto itOwned = m_systemGroups.find(group);
                if (itOwned != m_systemGroups.end()) {
                    for (auto& system : itOwned->second) {
                        updateSystem(system.get());
                    }
                }

                auto itScripted = m_scriptedSystemGroups.find(group);
                if (itScripted != m_scriptedSystemGroups.end()) {
                    for (auto* system : itScripted->second) {
                        updateSystem(system);
                    }
                }
                return;
            }

            const auto& graph = it->second;

            // Get execution levels (all systems per level can run in parallel)
            const auto& levels = graph.GetExecutionLevels();

            for (const auto& level : levels) {
                // Execute all systems in this level sequentially
                for (auto* system : level) {
                    updateSystem(system);
                }
            }
        }
    };

}

#endif
