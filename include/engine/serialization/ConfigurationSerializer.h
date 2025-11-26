#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <nlohmann/json.hpp>
#include "EntitySerializer.h"
#include "Serializer.h"

using json = nlohmann::json;

/**
 * @brief Editor configuration structure
 * 
 * This structure holds editor-specific settings loaded from config.json
 */
struct EditorSettings {
    /**
     * @brief Window-specific configuration settings
     */
    struct Window {
        int Width = 1600;           // Window width in pixels
        int Height = 900;           // Window height in pixels
        bool Maximized = true;      // Whether to start maximized
        bool VSync = true;          // Whether to enable vertical sync
    } WindowSettings;
};

/**
 * @brief Project configuration structure
 * 
 * This structure holds game project settings loaded from ProjectSettings.json
 */
struct ProjectSettings {
    /**
     * @brief Window-specific configuration settings
     */
    struct Window {
        int Width = 1600;           // Window width in pixels
        int Height = 900;           // Window height in pixels
        bool Fullscreen = true;     // Whether to start in fullscreen mode
        bool VSync = true;          // Whether to enable vertical sync
    } WindowSettings;

    /**
     * @brief Physics-specific configuration settings
     */
    struct Physics {
        float Gravity = -9.81f;    // Gravity acceleration
        float TimeStep = 0.016f;    // Physics time step
    } Physics;

    std::string Title = "GrapeEngine Game Project"; // Game/project name
    std::string Version = "1.0.0";                  // Version string
    std::string StartupScene = "";                  // Path to startup scene
};

namespace Serialization {
    /**
     * @brief Handles loading, saving, and validating application configuration files
     * 
     * This class provides functionality to load and save application settings from
     * JSON configuration files, with fallback to default values if the file cannot
     * be read or parsed. It also provides basic validation.
     */
    class ConfigurationSerializer {
    public:
        /**
         * @brief Loads application configuration from a JSON file
         * @param configPath Path to the configuration JSON file
         * @param config Reference to EditorSettings structure to populate
         * @return true if config was loaded successfully, false if using defaults
         */
        static bool LoadConfig(const std::string& configPath, EditorSettings& config) {
            json configJson;
            if (!Serializer::LoadJson(configPath, "json", configJson)) {
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

            LOG_DEBUG("Configuration loaded successfully from: " << configPath << '\n');
            return true;
        }

        /**
         * @brief Saves application configuration to a JSON file
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

            return Serializer::SaveJson(configPath, "json", configJson);
        }

        /**
         * @brief Loads project settings from a JSON file
         * @param settingsPath Path to the ProjectSettings.json file
         * @param settings Reference to ProjectSettings structure to populate
         * @return true if settings were loaded successfully
         */
        static bool LoadProjectSettings(const std::string& settingsPath, ProjectSettings& settings) {
            json settingsJson;
            if (!Serializer::LoadJson(settingsPath, "json", settingsJson)) {
                LOG_WARNING("Warning: Could not open project settings file: " << settingsPath);
                return false;
            }

            // Parse root-level fields
            if (settingsJson.contains("Title")) {
                settings.Title = settingsJson["Title"].get<std::string>();
            }
            if (settingsJson.contains("Version")) {
                settings.Version = settingsJson["Version"].get<std::string>();
            }
            if (settingsJson.contains("StartupScene")) {
                settings.StartupScene = settingsJson["StartupScene"].get<std::string>();
            }

            // Parse WindowSettings
            if (settingsJson.contains("WindowSettings")) {
                _parseWindowConfig(settingsJson["WindowSettings"], settings.WindowSettings);
            }

            // Parse Physics
            if (settingsJson.contains("Physics")) {
                const auto& physics = settingsJson["Physics"];
                if (physics.contains("Gravity")) {
                    settings.Physics.Gravity = physics["Gravity"].get<float>();
                }
                if (physics.contains("TimeStep")) {
                    settings.Physics.TimeStep = physics["TimeStep"].get<float>();
                }
            }

            LOG_DEBUG("Project settings loaded successfully from: " << settingsPath << '\n');
            return true;
        }

        /**
         * @brief Saves project settings to a JSON file
         * @param settingsPath Path to the ProjectSettings.json file
         * @param settings Reference to ProjectSettings structure to save
         * @return true if settings were saved successfully
         */
        static bool SaveProjectSettings(const std::string& settingsPath, const ProjectSettings& settings) {
            json settingsJson;
            settingsJson["Title"] = settings.Title;
            settingsJson["Version"] = settings.Version;
            settingsJson["StartupScene"] = settings.StartupScene;
            
            settingsJson["WindowSettings"]["Width"] = settings.WindowSettings.Width;
            settingsJson["WindowSettings"]["Height"] = settings.WindowSettings.Height;
            settingsJson["WindowSettings"]["Fullscreen"] = settings.WindowSettings.Fullscreen;
            settingsJson["WindowSettings"]["VSync"] = settings.WindowSettings.VSync;
            
            settingsJson["Physics"]["Gravity"] = settings.Physics.Gravity;
            settingsJson["Physics"]["TimeStep"] = settings.Physics.TimeStep;

            return Serializer::SaveJson(settingsPath, "json", settingsJson);
        }

        /**
         * @brief Validates the configuration structure
         * @param config Reference to EditorSettings structure to validate
         * @return true if valid, false otherwise
         */
        static bool ValidateConfig(const EditorSettings& config) {
            // Basic validation: width/height positive, name non-empty
            if (config.WindowSettings.Width <= 0 || config.WindowSettings.Height <= 0)
                return false;
            return true;
        }

        static bool ValidateProjectSettings(const ProjectSettings& settings) {
            // Basic validation: width/height positive, game name non-empty
            if (settings.WindowSettings.Width <= 0 || settings.WindowSettings.Height <= 0)
                return false;
            if (settings.Title.empty())
                return false;
            return true;
        }

        /**
         * @brief Returns a default application configuration
         * @return EditorSettings with default values
         */
        static EditorSettings GetDefaultConfig() {
            return EditorSettings{};
        }
        
    private:
        static void _parseWindowConfig(const json& configJson, EditorSettings::Window& window) {
            if (configJson.contains("Width")) {
                window.Width = configJson["Width"].get<int>();
            }
            if (configJson.contains("Height")) {
                window.Height = configJson["Height"].get<int>();
            }
            if (configJson.contains("Maximized")) {
                std::cout << "Maximized field found in config: " << configJson["Maximized"] << "\n";
                window.Maximized = configJson["Maximized"].get<bool>();
                std::cout << "Set Maximized to: " << window.Maximized << "\n";
            }
            if (configJson.contains("VSync")) {
                window.VSync = configJson["VSync"].get<bool>();
            }
        }

        static void _parseWindowConfig(const json& configJson, ProjectSettings::Window& window) {
            if (configJson.contains("Width")) {
                window.Width = configJson["Width"].get<int>();
            }
            if (configJson.contains("Height")) {
                window.Height = configJson["Height"].get<int>();
            }
            if (configJson.contains("Fullscreen")) {
                window.Fullscreen = configJson["Fullscreen"].get<bool>();
            }
            if (configJson.contains("VSync")) {
                window.VSync = configJson["VSync"].get<bool>();
            }
        }
    };
}

#endif