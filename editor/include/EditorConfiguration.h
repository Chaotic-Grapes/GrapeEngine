/* Start Header *****************************************************************/
/*!
\file   EditorConfiguration.h
\author Muhammad Nur Fadzly Bin Zulkifli (95%)
        Foo Rui Qin (5%)
\par    muhammadnurfadzly.b@digipen.edu
        ruiqin.foo@digipen.edu
\date   11th March 2026
\brief
Editor-specific configuration structure and serialization.
Separated from engine to maintain clean Engine/Editor architecture.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header *******************************************************************/

#ifndef EDITOR_CONFIGURATION_H
#define EDITOR_CONFIGURATION_H

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include "core/Logger.h"
#include "serialization/Serializer.h"

using json = nlohmann::json;

// Editor-only configuration structure holding settings loaded from config.json
// Kept separate from engine ProjectSettings to maintain architecture boundaries
struct EditorSettings {
    // Window-specific configuration settings for the editor
    struct Window {
        int Width = 1600;              // Window width in pixels
        int Height = 900;              // Window height in pixels
        bool Maximized = true;         // Whether to start maximized
        bool VSync = true;             // Whether to enable vertical sync
        std::string Mode = "Windowed"; // Display mode: Windowed, Fullscreen, Borderless
    } WindowSettings;

    float UiScale = 1.0f;                       // Global UI scale factor
    int AssetBrowserViewMode = 1;               // Asset browser view mode: 0 = List, 1 = Grid
    std::vector<std::string> RecentProjects;    // Ordered list of recently opened project paths
    std::string LastProject;                    // Path of the most recently opened project
};

namespace Editor {
    // Handles loading, saving, and validating editor configuration from JSON files
    // Falls back to default values when a config file is missing or malformed
    class EditorConfiguration {
    public:
        // Load editor configuration from a JSON file into config
        // Returns true if loaded successfully, false if falling back to defaults
        static bool LoadConfig(const std::string& configPath, EditorSettings& config) {
            json configJson;
            if (!Serialization::Serializer::LoadJson(configPath, "json", configJson)) {
                LOG_WARNING("Warning: Could not open config file: " << configPath);
                LOG_WARNING("Creating default configuration to use.");
                config = GetDefaultConfig();
                SaveConfig(configPath, config); // Save default config
                return false;
            }

            // Parse WindowSettings
            if (configJson.contains("WindowSettings")) {
                _parseWindowConfig(configJson["WindowSettings"], config.WindowSettings);
            }

            if (configJson.contains("RecentProjects") && configJson["RecentProjects"].is_array()) {
                config.RecentProjects.clear();
                for (const auto& entry : configJson["RecentProjects"]) {
                    if (entry.is_string()) {
                        config.RecentProjects.push_back(entry.get<std::string>());
                    }
                }
            }

            if (configJson.contains("UiScale") && configJson["UiScale"].is_number()) {
                config.UiScale = configJson["UiScale"].get<float>();
            }
            if (configJson.contains("AssetBrowserViewMode") && configJson["AssetBrowserViewMode"].is_number_integer()) {
                config.AssetBrowserViewMode = std::clamp(configJson["AssetBrowserViewMode"].get<int>(), 0, 1);
            }
            if (configJson.contains("LastProject") && configJson["LastProject"].is_string()) {
                config.LastProject = configJson["LastProject"].get<std::string>();
            }

            LOG_DEBUG("Editor configuration loaded successfully from: " << configPath << '\n');
            return true;
        }

        // Save editor configuration to a JSON file
        // Returns true if saved successfully, false otherwise
        static bool SaveConfig(const std::string& configPath, const EditorSettings& config) {
            json configJson;

            configJson["WindowSettings"]["Width"] = config.WindowSettings.Width;
            configJson["WindowSettings"]["Height"] = config.WindowSettings.Height;
            configJson["WindowSettings"]["Maximized"] = config.WindowSettings.Maximized;
            configJson["WindowSettings"]["VSync"] = config.WindowSettings.VSync;
            configJson["WindowSettings"]["Mode"] = _normalizeWindowMode(config.WindowSettings.Mode);
            configJson["UiScale"] = config.UiScale;
            configJson["AssetBrowserViewMode"] = std::clamp(config.AssetBrowserViewMode, 0, 1);
            configJson["RecentProjects"] = config.RecentProjects;
            configJson["LastProject"] = config.LastProject;

            return Serialization::Serializer::SaveJson(configPath, "json", configJson);
        }

        // Validate the configuration structure
        // Returns true if valid, false if width/height are non-positive or mode is unrecognized
        static bool ValidateConfig(const EditorSettings& config) {
            // Basic validation: width/height positive
            if (config.WindowSettings.Width <= 0 || config.WindowSettings.Height <= 0)
                return false;
            if (_normalizeWindowMode(config.WindowSettings.Mode).empty())
                return false;
            return true;
        }

        // Returns a default-constructed EditorSettings with all default values
        static EditorSettings GetDefaultConfig() {
            return EditorSettings{};
        }

    private:
        // Normalize a window mode string to one of: Windowed, Fullscreen, Borderless
        // Returns Windowed for any unrecognized input
        static std::string _normalizeWindowMode(const std::string& mode) {
            if (mode.empty()) {
                return "Windowed";
            }

            std::string lowered = mode;
            std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (lowered == "fullscreen" || lowered == "exclusive") {
                return "Fullscreen";
            }
            if (lowered == "borderlessfullscreen" || lowered == "borderless_fullscreen" || lowered == "borderless fullscreen" ||
                lowered == "borderless") {
                return "Borderless";
            }
            if (lowered == "windowed" || lowered == "window") {
                return "Windowed";
            }

            return "Windowed";
        }

        // Parse window configuration fields from JSON into the Window struct
        static void _parseWindowConfig(const json& configJson, EditorSettings::Window& window) {
            if (configJson.contains("Width")) {
                window.Width = configJson["Width"].get<int>();
            }
            if (configJson.contains("Height")) {
                window.Height = configJson["Height"].get<int>();
            }
            if (configJson.contains("Maximized")) {
                window.Maximized = configJson["Maximized"].get<bool>();
            }
            if (configJson.contains("VSync")) {
                window.VSync = configJson["VSync"].get<bool>();
            }
            if (configJson.contains("Mode")) {
                window.Mode = _normalizeWindowMode(configJson["Mode"].get<std::string>());
            }
        }
    };
}

#endif