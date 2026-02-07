/* Start Header *****************************************************************/
/*!
\file   EditorConfiguration.h
\author Muhammad Nur Fadzly Bin Zulkifli (100%)
\par    muhammadnurfadzly.b@digipen.edu
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
#include "core/Logger.h"
#include "serialization/Serializer.h"

using json = nlohmann::json;

/**
 * @brief Editor-only configuration structure
 * 
 * This structure holds editor-specific settings loaded from config.json
 * Kept separate from engine ProjectSettings to maintain architecture boundaries.
 */
struct EditorSettings {
    /**
     * @brief Window-specific configuration settings for editor
     */
    struct Window {
        int Width = 1600;           // Window width in pixels
        int Height = 900;           // Window height in pixels
        bool Maximized = true;      // Whether to start maximized
        bool VSync = true;          // Whether to enable vertical sync
        std::string Mode = "Windowed"; // Windowed, Fullscreen, Borderless
    } WindowSettings;
};

namespace Editor {
    /**
     * @brief Handles loading, saving, and validating editor configuration
     * 
     * Provides functionality to load and save editor settings from JSON
     * configuration files, with fallback to default values.
     */
    class EditorConfiguration {
    public:
        /**
         * @brief Loads editor configuration from a JSON file
         * @param configPath Path to the configuration JSON file
         * @param config Reference to EditorSettings structure to populate
         * @return true if config was loaded successfully, false if using defaults
         */
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

            LOG_DEBUG("Editor configuration loaded successfully from: " << configPath << '\n');
            return true;
        }

        /**
         * @brief Saves editor configuration to a JSON file
         * @param configPath Path to the configuration JSON file
         * @param config Reference to EditorSettings structure to save
         * @return true if config was saved successfully, false otherwise
         */
        static bool SaveConfig(const std::string& configPath, const EditorSettings& config) {
            json configJson;

            configJson["WindowSettings"]["Width"] = config.WindowSettings.Width;
            configJson["WindowSettings"]["Height"] = config.WindowSettings.Height;
            configJson["WindowSettings"]["Maximized"] = config.WindowSettings.Maximized;
            configJson["WindowSettings"]["VSync"] = config.WindowSettings.VSync;
            configJson["WindowSettings"]["Mode"] = _normalizeWindowMode(config.WindowSettings.Mode);

            return Serialization::Serializer::SaveJson(configPath, "json", configJson);
        }

        /**
         * @brief Validates the configuration structure
         * @param config Reference to EditorSettings structure to validate
         * @return true if valid, false otherwise
         */
        static bool ValidateConfig(const EditorSettings& config) {
            // Basic validation: width/height positive
            if (config.WindowSettings.Width <= 0 || config.WindowSettings.Height <= 0)
                return false;
            if (_normalizeWindowMode(config.WindowSettings.Mode).empty())
                return false;
            return true;
        }

        /**
         * @brief Returns a default editor configuration
         * @return EditorSettings with default values
         */
        static EditorSettings GetDefaultConfig() {
            return EditorSettings{};
        }
        
    private:
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
