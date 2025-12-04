/* Start Header *****************************************************************/
/*!
\file    SystemRegistry.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of the SystemRegistry class, which provides
a global registry for ECS systems. Systems are registered once with a name and
can be looked up and executed by scenes using SystemProfiles.

This supports a data-driven architecture where scenes specify which systems to
run via configuration rather than code inheritance.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef SYSTEMREGISTRY_H
#define SYSTEMREGISTRY_H

#include "ecs/World.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace Scenes {
    /**
     * @brief Function signature for ECS systems.
     * Systems receive a reference to the World and delta time.
     */
    using SystemFunction = std::function<void(ECS::World&, float)>;

    /**
     * @brief Internal structure holding system function and enabled state.
     */
    struct SystemEntry {
        SystemFunction function;
        bool enabled = true;
    };

    /**
     * @brief Global registry for ECS systems.
     * 
     * Provides static methods to register systems by name and retrieve them.
     * This allows scenes to specify which systems to run via data (SystemProfile)
     * rather than code inheritance.
     * 
     * Example usage:
     * @code
     * // During engine initialization:
     * SystemRegistry::Register("Physics", [](ECS::World& w, float dt) {
     *     ECS::PhysicsSystem::Update(w, dt);
     * });
     * 
     * SystemRegistry::Register("Render", [](ECS::World& w, float dt) {
     *     ECS::RendererSystem::Update(w, dt);
     * });
     * 
     * // Later, execute a system:
     * auto* system = SystemRegistry::Get("Physics");
     * if (system) {
     *     (*system)(world, deltaTime);
     * }
     * @endcode
     */
    class SystemRegistry {
    public:
        /**
         * @brief Registers a system with a unique name.
         * @param name The unique identifier for the system (e.g., "Physics", "Render").
         * @param system The system function to register.
         * @param enabled Initial enabled state (default: true).
         * @return True if registered successfully, false if name already exists.
         */
        static bool Register(const std::string& name, SystemFunction system, bool enabled = true) {
            auto& registry = _getRegistry();
            if (registry.find(name) != registry.end()) {
                return false; // Already registered
            }
            registry[name] = SystemEntry{std::move(system), enabled};
            return true;
        }

        /**
         * @brief Retrieves a registered system by name.
         * @param name The name of the system to retrieve.
         * @return Pointer to the system function, or nullptr if not found.
         * @note Returns the function regardless of enabled state, allowing manual calls.
         */
        static SystemFunction* Get(const std::string& name) {
            auto& registry = _getRegistry();
            auto it = registry.find(name);
            if (it != registry.end()) {
                return &it->second.function;
            }
            return nullptr;
        }

        /**
         * @brief Checks if a system is registered.
         * @param name The name of the system to check.
         * @return True if the system exists in the registry.
         */
        static bool Has(const std::string& name) {
            return _getRegistry().find(name) != _getRegistry().end();
        }

        /**
         * @brief Checks if a system is enabled.
         * @param name The name of the system to check.
         * @return True if the system exists and is enabled, false otherwise.
         */
        static bool IsEnabled(const std::string& name) {
            auto& registry = _getRegistry();
            auto it = registry.find(name);
            return it != registry.end() && it->second.enabled;
        }

        /**
         * @brief Enables a registered system.
         * @param name The name of the system to enable.
         * @return True if the system was found and enabled, false if not found.
         */
        static bool Enable(const std::string& name) {
            auto& registry = _getRegistry();
            auto it = registry.find(name);
            if (it != registry.end()) {
                it->second.enabled = true;
                return true;
            }
            return false;
        }

        /**
         * @brief Disables a registered system.
         * @param name The name of the system to disable.
         * @return True if the system was found and disabled, false if not found.
         * @note Disabled systems can still be called manually via Get().
         */
        static bool Disable(const std::string& name) {
            auto& registry = _getRegistry();
            auto it = registry.find(name);
            if (it != registry.end()) {
                it->second.enabled = false;
                return true;
            }
            return false;
        }

        /**
         * @brief Sets the enabled state of a system.
         * @param name The name of the system.
         * @param enabled The new enabled state.
         * @return True if the system was found, false otherwise.
         */
        static bool SetEnabled(const std::string& name, bool enabled) {
            auto& registry = _getRegistry();
            auto it = registry.find(name);
            if (it != registry.end()) {
                it->second.enabled = enabled;
                return true;
            }
            return false;
        }

        /**
         * @brief Unregisters a system by name.
         * @param name The name of the system to remove.
         * @return True if the system was removed, false if it didn't exist.
         */
        static bool Unregister(const std::string& name) {
            auto& registry = _getRegistry();
            return registry.erase(name) > 0;
        }

        /**
         * @brief Clears all registered systems.
         * @note Use with caution - typically only needed during engine shutdown.
         */
        static void Clear() {
            _getRegistry().clear();
        }

        /**
         * @brief Gets the count of registered systems.
         * @return The number of systems in the registry.
         */
        static size_t Count() {
            return _getRegistry().size();
        }

        /**
         * @brief Iterates over all registered systems.
         * @tparam TFn Function type accepting (const std::string& name, const SystemFunction& system).
         * @param fn The function to invoke for each system.
         * @param onlyEnabled If true, only iterates over enabled systems.
         */
        template<typename TFn>
        static void ForEach(TFn&& fn, bool onlyEnabled = false) {
            for (const auto& [name, entry] : _getRegistry()) {
                if (!onlyEnabled || entry.enabled) {
                    fn(name, entry.function);
                }
            }
        }

    private:
        // Singleton registry pattern to ensure single global instance
        static std::unordered_map<std::string, SystemEntry>& _getRegistry() {
            static std::unordered_map<std::string, SystemEntry> s_registry;
            return s_registry;
        }
    };
}

#endif
