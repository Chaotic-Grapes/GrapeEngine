/* Start Header *****************************************************************/
/*!
\file    SystemProfile.h
\author  Muhammad Nur Fadzly Bin Zulkifli (100%)
\par     muhammadnurfadzly.b@digipen.edu
\brief
This file contains the declaration of the SystemProfile struct, which defines
a data-driven configuration for which systems should run in a scene and their
execution settings.

SystemProfiles are serializable and allow scenes to be configured in a level
editor without code changes.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header
********************************************************************************/

#ifndef SYSTEMPROFILE_H
#define SYSTEMPROFILE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace Scenes {
    /**
     * @brief Defines which systems should run in a scene and their execution order.
     * 
     * SystemProfile is a pure data structure that can be serialized to/from JSON.
     * It specifies:
     * - Which systems to execute (by name)
     * - The execution order
     * - Whether each system is enabled/disabled
     * 
     * This allows scenes to be configured via data rather than code inheritance.
     * 
     * Example JSON representation:
     * @code{.json}
     * {
     *   "Systems": [
     *     { "Name": "Physics", "Enabled": true },
     *     { "Name": "Animation", "Enabled": true },
     *     { "Name": "Render", "Enabled": true },
     *     { "Name": "Debug", "Enabled": false }
     *   ]
     * }
     * @endcode
     */
    struct SystemProfile {
        /**
         * @brief Individual system entry in the profile.
         */
        struct SystemEntry {
            std::string Name;       ///< Name of the system (matches SystemRegistry)
            bool Enabled = true;    ///< Whether this system should execute

            // JSON serialization
            NLOHMANN_DEFINE_TYPE_INTRUSIVE(SystemEntry, Name, Enabled)
        };

        std::vector<SystemEntry> Systems;  ///< Ordered list of systems to execute

        /**
         * @brief Creates a default system profile with common systems.
         * @return A SystemProfile with Physics, Animation, and Render systems enabled.
         */
        static SystemProfile CreateDefault() {
            SystemProfile profile;
            profile.Systems.push_back({"Physics", true});
            profile.Systems.push_back({"Animation", true});
            profile.Systems.push_back({"Render", true});
            profile.Systems.push_back({ "Audio", true});
            return profile;
        }

        /**
         * @brief Adds a system to the profile.
         * @param name The name of the system (must exist in SystemRegistry).
         * @param enabled Whether the system is enabled by default.
         */
        void AddSystem(const std::string& name, bool enabled = true) {
            Systems.push_back({name, enabled});
        }

        /**
         * @brief Removes a system from the profile by name.
         * @param name The name of the system to remove.
         * @return True if a system was removed, false if not found.
         */
        bool RemoveSystem(const std::string& name) {
            auto it = std::remove_if(Systems.begin(), Systems.end(),
                [&name](const SystemEntry& entry) { return entry.Name == name; });
            if (it != Systems.end()) {
                Systems.erase(it, Systems.end());
                return true;
            }
            return false;
        }

        /**
         * @brief Enables or disables a system in the profile.
         * @param name The name of the system.
         * @param enabled The new enabled state.
         * @return True if the system was found and updated, false otherwise.
         */
        bool SetSystemEnabled(const std::string& name, bool enabled) {
            for (auto& entry : Systems) {
                if (entry.Name == name) {
                    entry.Enabled = enabled;
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Checks if a system is enabled in the profile.
         * @param name The name of the system.
         * @return True if the system exists and is enabled, false otherwise.
         */
        bool IsSystemEnabled(const std::string& name) const {
            for (const auto& entry : Systems) {
                if (entry.Name == name) {
                    return entry.Enabled;
                }
            }
            return false;
        }

        /**
         * @brief Gets the number of systems in the profile.
         * @return The count of systems.
         */
        size_t GetSystemCount() const {
            return Systems.size();
        }

        /**
         * @brief Clears all systems from the profile.
         */
        void Clear() {
            Systems.clear();
        }

        // JSON serialization support
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(SystemProfile, Systems)
    };
}

#endif
