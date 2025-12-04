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

#include "ecs/ISystem.h"
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
    class SystemManager {
    public:
        SystemManager() = default;
        ~SystemManager() = default;

        /**
         * @brief Register a new system.
         * @tparam T System type (must inherit from ISystem)
         * @tparam Args Constructor argument types
         * @param args Constructor arguments for the system
         * @return Pointer to the registered system
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

            // Store in appropriate group
            SystemGroup group = system->GetSystemGroup();
            m_systemGroups[group].push_back(std::move(system));

            // Track by name
            m_systemsByName[metadata.name] = ptr;

            // Sort systems by execution order within group
            _sortSystemGroup(group);

            return ptr;
        }

        /**
         * @brief Register a C# scripted system.
         * @param system Raw pointer to C# system wrapper (managed externally)
         * 
         * For C# systems, the lifetime is managed by the scripting layer.
         * The system is not owned by SystemManager.
         */
        void RegisterScriptedSystem(ISystem* system) {
            if (!system) return;

            const auto& metadata = system->GetMetadata();
            SystemGroup group = system->GetSystemGroup();

            m_scriptedSystemGroups[group].push_back(system);
            m_systemsByName[metadata.name] = system;

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
         * PreUpdate → Update → PostUpdate → PrePhysics → Physics → PostPhysics → PreRender → Render → PostRender
         * 
         * Within each group, systems are sorted by execution order (lower = earlier).
         */
        void Update(World& world, float deltaTime) {
            _updateGroup(SystemGroup::PreUpdate, world, deltaTime);
            _updateGroup(SystemGroup::Update, world, deltaTime);
            _updateGroup(SystemGroup::PostUpdate, world, deltaTime);
            _updateGroup(SystemGroup::PrePhysics, world, deltaTime);
            _updateGroup(SystemGroup::Physics, world, deltaTime);
            _updateGroup(SystemGroup::PostPhysics, world, deltaTime);
            _updateGroup(SystemGroup::PreRender, world, deltaTime);
            _updateGroup(SystemGroup::Render, world, deltaTime);
            _updateGroup(SystemGroup::PostRender, world, deltaTime);
        }

        /**
         * @brief Update systems in a specific group.
         * @param group System group to update
         * @param world Active scene's World
         * @param deltaTime Time since last frame
         */
        void UpdateGroup(SystemGroup group, World& world, float deltaTime) {
            _updateGroup(group, world, deltaTime);
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
        void UpdateSystemsForMode(SystemRunMode mode, World& world, float deltaTime) {
            _updateGroupForMode(SystemGroup::PreUpdate, mode, world, deltaTime);
            _updateGroupForMode(SystemGroup::Update, mode, world, deltaTime);
            _updateGroupForMode(SystemGroup::PostUpdate, mode, world, deltaTime);
            _updateGroupForMode(SystemGroup::PrePhysics, mode, world, deltaTime);
            _updateGroupForMode(SystemGroup::Physics, mode, world, deltaTime);
            _updateGroupForMode(SystemGroup::PostPhysics, mode, world, deltaTime);
            _updateGroupForMode(SystemGroup::PreRender, mode, world, deltaTime);
            _updateGroupForMode(SystemGroup::Render, mode, world, deltaTime);
            _updateGroupForMode(SystemGroup::PostRender, mode, world, deltaTime);
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

    private:
        /// Native C++ systems (owned)
        std::unordered_map<SystemGroup, std::vector<std::unique_ptr<ISystem>>> m_systemGroups;

        /// Scripted C# systems (not owned, managed by scripting layer)
        std::unordered_map<SystemGroup, std::vector<ISystem*>> m_scriptedSystemGroups;

        /// Name -> System lookup
        std::unordered_map<std::string, ISystem*> m_systemsByName;

        /**
         * @brief Sort systems within a group by execution order.
         */
        void _sortSystemGroup(SystemGroup group) {
            // Sort owned systems
            auto& systems = m_systemGroups[group];
            std::sort(systems.begin(), systems.end(),
                [](const std::unique_ptr<ISystem>& a, const std::unique_ptr<ISystem>& b) {
                    return a->GetMetadata().executionOrder < b->GetMetadata().executionOrder;
                });

            // Sort scripted systems
            auto& scriptedSystems = m_scriptedSystemGroups[group];
            std::sort(scriptedSystems.begin(), scriptedSystems.end(),
                [](const ISystem* a, const ISystem* b) {
                    return a->GetMetadata().executionOrder < b->GetMetadata().executionOrder;
                });
        }

        /**
         * @brief Update all systems in a specific group.
         */
        void _updateGroup(SystemGroup group, World& world, float deltaTime) {
            // Update owned systems
            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (system->IsEnabled()) {
                        system->OnUpdate(world, deltaTime);
                    }
                }
            }

            // Update scripted systems
            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (system->IsEnabled()) {
                        system->OnUpdate(world, deltaTime);
                    }
                }
            }
        }

        /**
         * @brief Update systems in a group filtered by run mode.
         */
        void _updateGroupForMode(SystemGroup group, SystemRunMode mode, World& world, float deltaTime) {
            // Update owned systems
            auto itOwned = m_systemGroups.find(group);
            if (itOwned != m_systemGroups.end()) {
                for (auto& system : itOwned->second) {
                    if (system->IsEnabled() && system->GetRunMode() == mode) {
                        system->OnUpdate(world, deltaTime);
                    }
                }
            }

            // Update scripted systems
            auto itScripted = m_scriptedSystemGroups.find(group);
            if (itScripted != m_scriptedSystemGroups.end()) {
                for (auto* system : itScripted->second) {
                    if (system->IsEnabled() && system->GetRunMode() == mode) {
                        system->OnUpdate(world, deltaTime);
                    }
                }
            }
        }
    };

}

#endif
